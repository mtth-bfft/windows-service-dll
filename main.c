#include <Windows.h>
#include <stdio.h>
#include <tchar.h>

const PCTSTR szServiceName = TEXT("MySvcDll");
const PCTSTR szServiceDisplayName = TEXT("My service DLL");
const PCTSTR szLogPath = TEXT("C:\\Windows\\Temp\\mysvcdll.log");

FILE* g_LogFile = NULL;
SERVICE_STATUS_HANDLE g_hSvcStatus = NULL;
SERVICE_STATUS g_svcStatus = { 0 };
HANDLE g_hStopEvent = NULL;

VOID WINAPI ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv);
int DoWork(DWORD argc, PWSTR argv[]);
DWORD InstallService(PCTSTR szCommand);
VOID Log(PCTSTR szFormat, ...);

// Entry point called whenever the DLL is loaded/unloaded, no matter the process
BOOL WINAPI DllMain(
   _In_ HINSTANCE hinstDLL,
   _In_ DWORD     dwReason,
   _In_ LPVOID    lpvReserved
)
{
   UNREFERENCED_PARAMETER(hinstDLL);
   UNREFERENCED_PARAMETER(lpvReserved);
   UNREFERENCED_PARAMETER(dwReason);
   return TRUE;
}

__declspec(dllexport) void RunServiceW(HWND hWnd, HINSTANCE hInst, LPSTR lpszCmdLine, int nCmdShow)
{
   UNREFERENCED_PARAMETER(hWnd);
   UNREFERENCED_PARAMETER(hInst);
   UNREFERENCED_PARAMETER(lpszCmdLine);
   UNREFERENCED_PARAMETER(nCmdShow);
   DWORD dwRes = 0;
   SERVICE_TABLE_ENTRY dispatchTable[2] = { 0 };

   // Open log file first
   if (szLogPath != NULL)
   {
      errno_t err = _tfopen_s(&g_LogFile, szLogPath, TEXT("a"));
      if (g_LogFile == NULL)
      {
         _ftprintf(stderr, TEXT("[!] Unable to open log file %s (err %d)\n"), szLogPath, err);
      }
   }

   dispatchTable[0].lpServiceName = (PTSTR)szServiceName;
   dispatchTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;

   // If called as a service, this call blocks synchronously until service exits
   if (StartServiceCtrlDispatcher(dispatchTable))
   {
      Log(TEXT("[.] Service exits cleanly"));
   }
   else
   {
      dwRes = GetLastError();
      Log(TEXT("[!] Service failed to communicate with SCM: StartServiceCtrlDispatcher() failed with code %u"), dwRes);
   }
   if (g_LogFile != NULL)
      fclose(g_LogFile);
}

__declspec(dllexport) void InstallServiceW(HWND hWnd, HINSTANCE hInst, LPWSTR szCmdLine, int nCmdShow)
{
   UNREFERENCED_PARAMETER(hWnd);
   UNREFERENCED_PARAMETER(hInst);
   UNREFERENCED_PARAMETER(szCmdLine);
   UNREFERENCED_PARAMETER(nCmdShow);
   TCHAR szPath[MAX_PATH + 3] = { 0 };
   TCHAR szCommand[MAX_PATH + 3] = { 0 };
   HMODULE hDll = NULL;

   // Open log file first
   if (szLogPath != NULL)
   {
      errno_t err = _tfopen_s(&g_LogFile, szLogPath, TEXT("a"));
      if (g_LogFile == NULL)
      {
         _ftprintf(stderr, TEXT("[!] Unable to open log file %s (err %d)\n"), szLogPath, err);
      }
   }

   GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&InstallServiceW, &hDll);
   GetModuleFileName(hDll, szPath, sizeof(szPath) / sizeof(szPath[0]));
   Log(TEXT("[.] Will install DLL %s as service \"%s\""), szPath, szServiceName);

   _stprintf_s(szCommand, sizeof(szCommand) / sizeof(szCommand[0]), TEXT("C:\\Windows\\System32\\rundll32.exe \"%s\",RunService"), szPath);

   InstallService(szCommand);

   if (g_LogFile != NULL)
      fclose(g_LogFile);
}

__declspec(dllexport) void RunInteractiveW(HWND hWnd, HINSTANCE hInst, LPWSTR szCmdLine, int nCmdShow)
{
   UNREFERENCED_PARAMETER(hWnd);
   UNREFERENCED_PARAMETER(hInst);
   UNREFERENCED_PARAMETER(nCmdShow);

   int argc = 0;
   PWSTR *argv = NULL;

   // Open log file first
   if (szLogPath != NULL)
   {
      errno_t err = _tfopen_s(&g_LogFile, szLogPath, TEXT("a"));
      if (g_LogFile == NULL)
      {
         _ftprintf(stderr, TEXT("[!] Unable to open log file %s (err %d)\n"), szLogPath, err);
      }
   }
   Log(TEXT("[.] Starting as interactive process"));

   argv = CommandLineToArgvW(szCmdLine, &argc);
   if (argv == NULL)
   {
      Log(TEXT("[!] CommandLineToArgvW() failed with code %u"), GetLastError());
      return;
   }

   DoWork(argc, argv);

   Log(TEXT("[.] Interactive process exits cleanly"));

   if (g_LogFile != NULL)
      fclose(g_LogFile);
}

BOOL ReportSvcStatus(DWORD dwCurrentState,
                     DWORD dwWin32ExitCode,
                     DWORD dwWaitHint)
{
   // Incremented counter
   static DWORD dwCheckPoint = 0;

   g_svcStatus.dwCurrentState = dwCurrentState;
   g_svcStatus.dwWin32ExitCode = dwWin32ExitCode;
   g_svcStatus.dwWaitHint = dwWaitHint;

   if (dwCurrentState == SERVICE_START_PENDING)
      g_svcStatus.dwControlsAccepted = 0;
   else
      g_svcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

   if (dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED)
      g_svcStatus.dwCheckPoint = 0;
   else
      g_svcStatus.dwCheckPoint = ++dwCheckPoint;

   // Report the status of the service to the SCM.
   return SetServiceStatus(g_hSvcStatus, &g_svcStatus);
}

VOID WINAPI SvcCtrlHandler(DWORD dwCtrl)
{
   switch (dwCtrl)
   {
   case SERVICE_CONTROL_STOP:
      Log(TEXT("[.] Exit requested by service manager"));
      ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 3000);
      if (g_hStopEvent != NULL)
         SetEvent(g_hStopEvent);
      break;
   default:
      break;
   }
}

VOID WINAPI ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
   int res = 0;

   // Initialize an event to signal we need to exit
   g_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
   if (g_hStopEvent == NULL)
   {
      Log(TEXT("[!] CreateEvent() failed with code %u"), GetLastError());
      return;
   }

   g_hSvcStatus = RegisterServiceCtrlHandler(szServiceName, SvcCtrlHandler);
   if (g_hSvcStatus == NULL)
   {
      Log(TEXT("[!] RegisterServiceCtrlHandler() failed with code %u"), GetLastError());
      return;
   }

   g_svcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
   if (!ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0))
   {
      Log(TEXT("[!] ReportSvcStatus(SERVICE_RUNNING) failed with code %u"), GetLastError());
      return;
   }

   res = DoWork(dwArgc, lpszArgv);

   if (!ReportSvcStatus(SERVICE_STOPPED, res, 0))
   {
      Log(TEXT("[!] ReportSvcStatus(SERVICE_STOPPED) failed with code %u"), GetLastError());
      return;
   }
}

DWORD InstallService(PCTSTR szCommand)
{
   DWORD dwRes = 0;
   SC_HANDLE hSCM = NULL;
   SC_HANDLE hService = NULL;

   hSCM = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
   if (hSCM == NULL)
   {
      dwRes = GetLastError();
      Log(TEXT("[!] OpenSCManager() failed with code %u"), dwRes);
      goto cleanup;
   }

   hService = CreateService(hSCM,
                            szServiceName,
                            szServiceDisplayName,
                            0,
                            SERVICE_WIN32_OWN_PROCESS,
                            SERVICE_DEMAND_START,
                            SERVICE_ERROR_IGNORE,
                            szCommand,
                            NULL, NULL, NULL, NULL, NULL);
   if (hService == NULL)
   {
      dwRes = GetLastError();
      Log(TEXT("[!] CreateService() failed with code %u"), dwRes);
      goto cleanup;
   }

   Log(TEXT("[+] Service installed"));

cleanup:
   if (hService != NULL)
      CloseServiceHandle(hService);
   if (hSCM != NULL)
      CloseServiceHandle(hSCM);
   return dwRes;
}

BOOL IsShutdownRequested()
{
   return (WaitForSingleObject(g_hStopEvent, 0) == WAIT_OBJECT_0);
}

VOID Log(PCTSTR szFormat, ...)
{
   SYSTEMTIME sysTime = { 0 };
   FILE* fOutput = (g_LogFile == NULL ? stdout : g_LogFile);
   va_list args;
   va_start(args, szFormat);

   GetSystemTime(&sysTime);

   _ftprintf(fOutput, TEXT("%04u-%02u-%02u %02u:%02u:%02u "),
             sysTime.wYear, sysTime.wMonth, sysTime.wDay,
             sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
   _vftprintf(fOutput, szFormat, args);
   _ftprintf(fOutput, TEXT("\n"));
   fflush(fOutput);

   va_end(args);
}

int DoWork(DWORD argc, PWSTR argv[])
{
   Log(TEXT("[.] Starting work"));
   do
   {
      // Actual work goes here
      UNREFERENCED_PARAMETER(argc);
      UNREFERENCED_PARAMETER(argv);

      Sleep(1000);
      if (IsShutdownRequested())
         break;
   } while (1);
   Log(TEXT("[.] Done"));
   return 0;
}