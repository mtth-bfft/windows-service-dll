# Minimal Windows Service as DLL

Just a template of Windows Service, packaged as a dynamic library (DLL). Why, I hear you ask? Well I wanted to know if that could work. The answer is yes.

## Usage

Actual work goes into DoWork(argc, argv). The example will just loop and wait for a stop request.

Service can be installed using `rundll32.exe .\minimal_service.dll,InstallService` (or `sc create MySvcDll binPath= C:\Windows\System32\rundll32.exe "C:\...\minimal_service.dll",RunService`)

The service can then be run using `sc start MySvc` (it will log to the `szLogPath` file), or interactively using `rundll32.exe .\minimal_service.dll,RunInteractive`
