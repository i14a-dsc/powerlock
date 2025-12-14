#include <windows.h>
#include <powrprof.h>
#include <shlobj.h>
#include <stdbool.h>
#include <stdio.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wtsapi32.lib")

#include <wtsapi32.h>

#define SERVICE_NAME "PowerLockService"
#define SERVICE_DISPLAY_NAME "AC Power Lock"
#define CONFIG_FILE "C:\\Data\\power.cfg"

SERVICE_STATUS g_ServiceStatus = {0};
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;
bool g_AutoLockEnabled = true;
  
VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);
void WriteToLog(const char *message);
bool is_on_ac_power();
void lock_computer();
void create_config_file();
bool load_config();
void create_data_directory();
void restart_service();

void WriteToLog(const char *message) {
  HANDLE hEventSource = RegisterEventSource(NULL, SERVICE_NAME);
  if (hEventSource != NULL) {
    const char *strings[1] = {message};
    ReportEvent(hEventSource, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0,
                strings, NULL);
    DeregisterEventSource(hEventSource);
  }
}

bool is_on_ac_power() {
  SYSTEM_POWER_STATUS power_status;
  if (GetSystemPowerStatus(&power_status)) {
    return (power_status.ACLineStatus == 1);
  }
  return true;
}

void create_data_directory() { CreateDirectory("C:\\Data", NULL); }

void create_config_file() {
  create_data_directory();

  FILE *file = fopen(CONFIG_FILE, "w");
  if (file != NULL) {
    fprintf(file, "[Power]\n");
    fprintf(file, "AutoLock=true\n");
    fclose(file);
    WriteToLog("Created default config file: C:\\Data\\power.cfg");
  } else {
    WriteToLog("Failed to create config file");
  }
}

bool load_config() {
  FILE *file = fopen(CONFIG_FILE, "r");
  if (file == NULL) {
    WriteToLog("Config file not found, creating default config");
    create_config_file();
    g_AutoLockEnabled = true;
    return true;
  }

  char line[256];
  bool found_autolock = false;

  while (fgets(line, sizeof(line), file)) {

    line[strcspn(line, "\r\n")] = 0;

    if (line[0] == '\0' || line[0] == '#' || line[0] == '[') {
      continue;
    }

    if (strncmp(line, "AutoLock=", 9) == 0) {
      char *value = line + 9;
      if (strcmp(value, "true") == 0) {
        g_AutoLockEnabled = true;
      } else if (strcmp(value, "false") == 0) {
        g_AutoLockEnabled = false;
      }
      found_autolock = true;
      break;
    }
  }

  fclose(file);

  if (!found_autolock) {
    WriteToLog("AutoLock setting not found in config, using default (true)");
    g_AutoLockEnabled = true;
  }

  char log_msg[256];
  sprintf(log_msg, "Config loaded: AutoLock=%s",
          g_AutoLockEnabled ? "true" : "false");
  WriteToLog(log_msg);

  return true;
}

void restart_service() {
  SC_HANDLE schSCManager;
  SC_HANDLE schService;

  schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (schSCManager == NULL) {
    printf("OpenSCManager failed (%lu)\n", GetLastError());
    return;
  }

  schService = OpenService(schSCManager, SERVICE_NAME, SERVICE_ALL_ACCESS);
  if (schService == NULL) {
    printf("OpenService failed (%lu)\n", GetLastError());
    CloseServiceHandle(schSCManager);
    return;
  }

  SERVICE_STATUS status;

  printf("Stopping service...\n");
  if (ControlService(schService, SERVICE_CONTROL_STOP, &status)) {

    while (QueryServiceStatus(schService, &status)) {
      if (status.dwCurrentState == SERVICE_STOPPED) {
        break;
      }
      Sleep(1000);
    }
    printf("Service stopped.\n");
  }

  printf("Starting service...\n");
  if (StartService(schService, 0, NULL)) {
    printf("Service restarted successfully.\n");
  } else {
    printf("Failed to start service (%lu)\n", GetLastError());
  }

  CloseServiceHandle(schService);
  CloseServiceHandle(schSCManager);
}

void lock_computer() {
  if (!g_AutoLockEnabled) {
    WriteToLog("AutoLock disabled - Not locking workstation");
    return;
  }

  WriteToLog("AC Adapter disconnected - Attempting to lock (disconnect) workstation");

  DWORD sessionId = WTSGetActiveConsoleSessionId();
  if (sessionId == 0xFFFFFFFF) {
     WriteToLog("No active console session found to lock");
     return;
  }

  if (!WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE, sessionId, FALSE)) {
     char msg[128];
     sprintf(msg, "WTSDisconnectSession failed: %lu", GetLastError());
     WriteToLog(msg);
      
     // Fallback to LockWorkStation just in case running in console mode or it works
     if (!LockWorkStation()) {
         WriteToLog("Fallback LockWorkStation also failed");
     }
  } else {
     WriteToLog("Session disconnected (Locked)");
  }
}

VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode) {
  switch (CtrlCode) {
  case SERVICE_CONTROL_STOP:
    WriteToLog("Service stop requested");
    if (g_ServiceStatus.dwCurrentState != SERVICE_STOP_PENDING) {
      g_ServiceStatus.dwControlsAccepted = 0;
      g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
      g_ServiceStatus.dwWin32ExitCode = 0;
      g_ServiceStatus.dwCheckPoint = 4;

      if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE) {
        WriteToLog("SetServiceStatus failed");
      }

      SetEvent(g_ServiceStopEvent);
    }
    break;

  default:
    break;
  }
}

DWORD WINAPI ServiceWorkerThread(LPVOID lpParam) {
  UNREFERENCED_PARAMETER(lpParam);

  WriteToLog("Power Lock started");

  load_config();

  bool previous_ac_state = is_on_ac_power();
  WriteToLog(previous_ac_state ? "Started on AC power" : "Started on battery power");

  while (WaitForSingleObject(g_ServiceStopEvent, 2000) == WAIT_TIMEOUT) {
    bool current_ac_state = is_on_ac_power();

    if (previous_ac_state && !current_ac_state) {
      lock_computer();

      Sleep(5000);
    }

    previous_ac_state = current_ac_state;
  }

  WriteToLog("Power Lock stopped");
  return ERROR_SUCCESS;
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv) {
  DWORD Status = E_FAIL;
  UNREFERENCED_PARAMETER(argc);
  UNREFERENCED_PARAMETER(argv);

  g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);

  if (g_StatusHandle == NULL) {
    return;
  }

  ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
  g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  g_ServiceStatus.dwControlsAccepted = 0;
  g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
  g_ServiceStatus.dwWin32ExitCode = 0;
  g_ServiceStatus.dwServiceSpecificExitCode = 0;
  g_ServiceStatus.dwCheckPoint = 0;

  if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE) {
    WriteToLog("SetServiceStatus failed");
  }

  g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (g_ServiceStopEvent == NULL) {
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = GetLastError();
    g_ServiceStatus.dwCheckPoint = 1;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE) {
      WriteToLog("SetServiceStatus failed");
    }
    return;
  }

  g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
  g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
  g_ServiceStatus.dwWin32ExitCode = 0;
  g_ServiceStatus.dwCheckPoint = 0;

  if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE) {
    WriteToLog("SetServiceStatus failed");
  }

  HANDLE hThread = CreateThread(
      NULL, 0, (LPTHREAD_START_ROUTINE)ServiceWorkerThread, NULL, 0, NULL);
  if (hThread != NULL) {

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
  }

  CloseHandle(g_ServiceStopEvent);

  g_ServiceStatus.dwControlsAccepted = 0;
  g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
  g_ServiceStatus.dwWin32ExitCode = 0;
  g_ServiceStatus.dwCheckPoint = 3;

  if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE) {
    WriteToLog("SetServiceStatus failed");
  }
}

int main(int argc, char *argv[]) {
  if (argc > 1) {
    if (strcmp(argv[1], "reload") == 0) {
      restart_service();
      return 0;
    } else if (strcmp(argv[1], "console") == 0) {

      printf("Power Lock Monitor - Console Mode\n");
      printf("Press Ctrl+C to exit\n\n");

      load_config();
      printf("AutoLock enabled: %s\n", g_AutoLockEnabled ? "Yes" : "No");

      bool previous_ac_state = is_on_ac_power();
      printf("Initial power status: %s\n",
             previous_ac_state ? "AC Power" : "Battery Power");

      printf("\nMonitoring power changes...\n");

      while (true) {
        Sleep(2000);
        bool current_ac_state = is_on_ac_power();

        if (previous_ac_state && !current_ac_state) {
          if (g_AutoLockEnabled) {
              printf("AC disconnected - Would lock computer (disabled in "
                     "console mode)\n");
          } else {
            printf("AC disconnected - AutoLock disabled, not locking\n");
          }
        }

        if (current_ac_state != previous_ac_state) {
          printf("Power state changed: %s\n",
                 current_ac_state ? "AC Power" : "Battery Power");
        }
        previous_ac_state = current_ac_state;
      }
    } else if (strcmp(argv[1], "config") == 0) {
      printf("Current configuration:\n");
      load_config();
      printf("AutoLock: %s\n", g_AutoLockEnabled ? "true" : "false");
      printf("Config file: %s\n", CONFIG_FILE);
      return 0;
    }
  }

  SERVICE_TABLE_ENTRY ServiceTable[] = {
      {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain}, {NULL, NULL}};

  if (StartServiceCtrlDispatcher(ServiceTable) == FALSE) {
    printf("StartServiceCtrlDispatcher failed (%lu)\n", GetLastError());
    printf("\nThis is a Windows service daemon.\n");
    printf("Use 'powerlock' command for service management.\n");
    printf("\nAvailable options:\n");
    printf("  %s reload    - Restart service to reload config\n", argv[0]);
    printf("  %s console   - Run in console mode for testing\n", argv[0]);
    printf("  %s config    - Show current configuration\n", argv[0]);
    printf("\nFor service installation/uninstallation, use:\n");
    printf("  powerlock install\n");
    printf("  powerlock uninstall\n");
    return GetLastError();
  }

  return 0;
}