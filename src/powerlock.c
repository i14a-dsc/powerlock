#include <windows.h>
#include <shellapi.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <tchar.h>


#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

#define CONFIG_FILE "C:\\Data\\power.cfg"
#define SERVICE_NAME "PowerLockService"

#define COLOR_RESET 7
#define COLOR_RED 12
#define COLOR_GREEN 10
#define COLOR_YELLOW 14
#define COLOR_CYAN 11

void show_usage();
void enable_autolock();
void disable_autolock();
void reload_service();
void show_status();
void open_config();
void create_data_directory();
bool service_exists();
void query_service_status();
void set_console_color(int color);
void print_warning(const char *message);
void print_success(const char *message);
void print_error(const char *message);
bool is_admin();
bool elevate_if_needed(int argc, char *argv[]);
void install_service();
void uninstall_service();

void show_usage() {
  printf("PowerLock Service Manager\n");
  printf("\n");
  printf("Usage: powerlock [command]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  enable     - Enable auto-lock (set AutoLock=true)\n");
  printf("  disable    - Disable auto-lock (set AutoLock=false)\n");
  printf("  reload     - Restart service to reload configuration (requires "
         "admin)\n");
  printf("  status     - Show current service status and configuration\n");
  printf("  config     - Open config file in notepad\n");
  printf("  install    - Install service (requires admin)\n");
  printf("  uninstall  - Uninstall service (requires admin)\n");
  printf("  help       - Show this help\n");
  printf("\n");
  printf("Config file: %s\n", CONFIG_FILE);
}

void set_console_color(int color) {
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(hConsole, color);
}

void print_warning(const char *message) {
  set_console_color(COLOR_YELLOW);
  printf("Warning: %s\n", message);
  set_console_color(COLOR_RESET);
}

void print_success(const char *message) {
  set_console_color(COLOR_GREEN);
  printf("Success: %s\n", message);
  set_console_color(COLOR_RESET);
}

void print_error(const char *message) {
  set_console_color(COLOR_RED);
  printf("Error: %s\n", message);
  set_console_color(COLOR_RESET);
}

bool is_admin() {
  BOOL isAdmin = FALSE;
  PSID adminGroup = NULL;
  SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

  if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                               DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                               &adminGroup)) {
    CheckTokenMembership(NULL, adminGroup, &isAdmin);
    FreeSid(adminGroup);
  }

  return isAdmin;
}

bool elevate_if_needed(int argc, char *argv[]) {
  if (is_admin()) {
    return true;
  }

  char cmdline[1024] = "";
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--wait") == 0) continue; // Don't duplicate wait flag
    if (i > 1)
      strcat(cmdline, " ");
    strcat(cmdline, argv[i]);
  }
  strcat(cmdline, " --wait");

  printf("Administrator privileges required. Requesting elevation...\n");

  SHELLEXECUTEINFO sei = {0};
  sei.cbSize = sizeof(sei);
  sei.lpVerb = "runas";
  sei.lpFile = argv[0];
  sei.lpParameters = cmdline;
  sei.nShow = SW_NORMAL;
  sei.fMask = SEE_MASK_NOCLOSEPROCESS;

  if (ShellExecuteEx(&sei)) {
    if (sei.hProcess) {
      WaitForSingleObject(sei.hProcess, INFINITE);
      CloseHandle(sei.hProcess);
    }
    return false;
  } else {
    print_error("Failed to request elevation");
    return false;
  }
}



void install_service() {
  SC_HANDLE schSCManager;
  SC_HANDLE schService;
  TCHAR szPath[MAX_PATH];

  if (!GetModuleFileName(NULL, szPath, MAX_PATH)) {
    print_error("Cannot get current executable path");
    return;
  }

  TCHAR *lastSlash = _tcsrchr(szPath, '\\');
  if (lastSlash) {
    *(lastSlash + 1) = '\0';
    _tcscat(szPath, TEXT("power_lock_service.exe"));
  } else {
    _tcscpy(szPath, TEXT("power_lock_service.exe"));
  }

  if (GetFileAttributes(szPath) == INVALID_FILE_ATTRIBUTES) {
    print_error("power_lock_service.exe not found in the same directory");
    return;
  }

  printf("Installing PowerLock Service...\n");

  schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

  if (NULL == schSCManager) {
    print_error("OpenSCManager failed. Run as Administrator.");
    return;
  }

  schService = CreateService(schSCManager, SERVICE_NAME, TEXT("AC Power Lock"),
                             SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                             SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, szPath,
                             NULL, NULL, NULL, NULL, NULL);

  if (schService == NULL) {
    DWORD error = GetLastError();
    if (error == ERROR_SERVICE_EXISTS) {
      print_warning("Service already exists");
    } else if (error == 1072) { // ERROR_SERVICE_MARKED_FOR_DELETE
      print_error("The specified service has been marked for deletion.");
      printf("This often happens if the Services management console (services.msc) is open.\n");
      printf("Please close all service management windows and retry, or restart the computer.\n");
    } else {
      print_error("CreateService failed");
      printf("Error code: %lu\n", error);
    }
    CloseServiceHandle(schSCManager);
    return;
  }

  print_success("Service installed successfully");

  printf("Starting service...\n");
  if (StartService(schService, 0, NULL)) {
    print_success("Service started successfully");
  } else {
    DWORD error = GetLastError();
    if (error == ERROR_SERVICE_ALREADY_RUNNING) {
      printf("Service is already running\n");
    } else {
      print_warning("Service installed but failed to start");
      printf("You can start it manually: sc start PowerLockService\n");
    }
  }

  CloseServiceHandle(schService);
  CloseServiceHandle(schSCManager);
}

void uninstall_service() {
  SC_HANDLE schSCManager;
  SC_HANDLE schService;

  printf("Uninstalling PowerLock Service...\n");

  schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

  if (NULL == schSCManager) {
    print_error("OpenSCManager failed. Run as Administrator.");
    return;
  }

  schService = OpenService(schSCManager, SERVICE_NAME, DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);

  if (schService == NULL) {
    DWORD error = GetLastError();
    if (error == ERROR_SERVICE_DOES_NOT_EXIST) {
      print_warning("Service does not exist");
    } else {
      print_error("OpenService failed");
      printf("Error code: %lu\n", error);
    }
    CloseServiceHandle(schSCManager);
    return;
  }

  SERVICE_STATUS status;
  if (ControlService(schService, SERVICE_CONTROL_STOP, &status)) {
    printf("Stopping service...\n");
    int timeout = 30;
    while (timeout > 0) {
      if (QueryServiceStatus(schService, &status)) {
        if (status.dwCurrentState == SERVICE_STOPPED) {
          break;
        }
      }
      Sleep(1000);
      timeout--;
    }
  } else {
      DWORD error = GetLastError();
      if (error != ERROR_SERVICE_NOT_ACTIVE) {
           printf("Note: Failed to signal service stop (error %lu), proceeding with uninstall...\n", error);
      }
  }

  if (!DeleteService(schService)) {
    print_error("DeleteService failed");
    printf("Error code: %lu\n", GetLastError());
  } else {
    print_success("Service uninstalled successfully");
  }

  CloseServiceHandle(schService);
  CloseServiceHandle(schSCManager);
}

void create_data_directory() { CreateDirectory("C:\\Data", NULL); }

void enable_autolock() {
  printf("Enabling auto-lock...\n");
  create_data_directory();

  FILE *file = fopen(CONFIG_FILE, "w");
  if (file != NULL) {
    fprintf(file, "[Power]\n");
    fprintf(file, "AutoLock=true\n");
    fclose(file);
    print_success(
        "Auto-lock enabled. Run 'powerlock reload' to apply changes.");
  } else {
    print_error("Failed to write config file.");
  }
}

void disable_autolock() {
  printf("Disabling auto-lock...\n");
  create_data_directory();

  FILE *file = fopen(CONFIG_FILE, "w");
  if (file != NULL) {
    fprintf(file, "[Power]\n");
    fprintf(file, "AutoLock=false\n");
    fclose(file);
    print_success(
        "Auto-lock disabled. Run 'powerlock reload' to apply changes.");
  } else {
    print_error("Failed to write config file.");
  }
}

bool service_exists() {
  SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
  if (schSCManager == NULL) {
    return false;
  }

  SC_HANDLE schService =
      OpenService(schSCManager, SERVICE_NAME, SERVICE_QUERY_STATUS);
  bool exists = (schService != NULL);

  if (schService)
    CloseServiceHandle(schService);
  CloseServiceHandle(schSCManager);

  return exists;
}

void query_service_status() {
  SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
  if (schSCManager == NULL) {
    printf("Failed to connect to Service Control Manager.\n");
    return;
  }

  SC_HANDLE schService =
      OpenService(schSCManager, SERVICE_NAME, SERVICE_QUERY_STATUS);
  if (schService == NULL) {
    printf("Service '%s' is not installed.\n", SERVICE_NAME);
    CloseServiceHandle(schSCManager);
    return;
  }

  SERVICE_STATUS status;
  if (QueryServiceStatus(schService, &status)) {
    printf("Service Status: ");
    switch (status.dwCurrentState) {
    case SERVICE_STOPPED:
      printf("STOPPED\n");
      break;
    case SERVICE_START_PENDING:
      printf("START_PENDING\n");
      break;
    case SERVICE_STOP_PENDING:
      printf("STOP_PENDING\n");
      break;
    case SERVICE_RUNNING:
      printf("RUNNING\n");
      break;
    case SERVICE_CONTINUE_PENDING:
      printf("CONTINUE_PENDING\n");
      break;
    case SERVICE_PAUSE_PENDING:
      printf("PAUSE_PENDING\n");
      break;
    case SERVICE_PAUSED:
      printf("PAUSED\n");
      break;
    default:
      printf("UNKNOWN (%lu)\n", status.dwCurrentState);
      break;
    }
  } else {
    printf("Failed to query service status.\n");
  }

  CloseServiceHandle(schService);
  CloseServiceHandle(schSCManager);
}

void reload_service() {
  if (!service_exists()) {
    print_warning("Service 'PowerLockService' is not installed.");
    printf("Install the service first with: powerlock install\n");
    return;
  }

  printf("Reloading PowerLock service...\n");

  SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (schSCManager == NULL) {
    print_error("Failed to connect to Service Control Manager. Administrator "
                "privileges required.");
    return;
  }

  SC_HANDLE schService =
      OpenService(schSCManager, SERVICE_NAME, SERVICE_ALL_ACCESS);
  if (schService == NULL) {
    print_error("Failed to open service 'PowerLockService'.");
    CloseServiceHandle(schSCManager);
    return;
  }

  SERVICE_STATUS status;

  printf("Stopping service...\n");
  if (ControlService(schService, SERVICE_CONTROL_STOP, &status)) {

    int timeout = 30;
    while (timeout > 0) {
      if (QueryServiceStatus(schService, &status)) {
        if (status.dwCurrentState == SERVICE_STOPPED) {
          break;
        }
      }
      Sleep(1000);
      timeout--;
    }

    if (status.dwCurrentState == SERVICE_STOPPED) {
      printf("Service stopped.\n");
    } else {
      printf("Warning: Service may not have stopped completely.\n");
    }
  } else {
    DWORD error = GetLastError();
    if (error == ERROR_SERVICE_NOT_ACTIVE) {
      printf("Service was already stopped.\n");
    } else {
      printf("Failed to stop service (error %lu).\n", error);
    }
  }

  printf("Starting service...\n");
  if (StartService(schService, 0, NULL)) {
    print_success("Service restarted successfully.");
  } else {
    DWORD error = GetLastError();
    if (error == ERROR_SERVICE_ALREADY_RUNNING) {
      printf("Service is already running.\n");
    } else {
      print_error("Failed to start service.");
      printf("Error code: %lu\n", error);
    }
  }

  CloseServiceHandle(schService);
  CloseServiceHandle(schSCManager);
}

void show_status() {
  printf("PowerLock Service Status:\n");
  printf("\n");

  if (!service_exists()) {
    print_warning("Service 'PowerLockService' is not installed.");
    printf("Install the service first with: powerlock install\n");
    printf("\n");
  } else {
    query_service_status();
  }

  printf("\nConfiguration:\n");

  FILE *file = fopen(CONFIG_FILE, "r");
  if (file != NULL) {
    char line[256];
    bool found_autolock = false;

    while (fgets(line, sizeof(line), file)) {

      line[strcspn(line, "\r\n")] = 0;

      if (line[0] == '\0' || line[0] == '#' || line[0] == '[') {
        continue;
      }

      if (strncmp(line, "AutoLock=", 9) == 0) {
        char *value = line + 9;
        printf("AutoLock: %s\n", value);
        found_autolock = true;
        break;
      }
    }

    if (!found_autolock) {
      printf("AutoLock: not configured (default: true)\n");
    }

    fclose(file);
  } else {
    printf("Config file not found: %s\n", CONFIG_FILE);
    printf("AutoLock: not configured (default: true)\n");
  }

  printf("Config file: %s\n", CONFIG_FILE);
}

void open_config() {
  printf("Opening config file...\n");

  FILE *file = fopen(CONFIG_FILE, "r");
  if (file == NULL) {
    printf("Config file not found. Creating default config...\n");
    create_data_directory();

    file = fopen(CONFIG_FILE, "w");
    if (file != NULL) {
      fprintf(file, "[Power]\n");
      fprintf(file, "AutoLock=true\n");
      fclose(file);
    } else {
      printf("Error: Failed to create config file.\n");
      return;
    }
  } else {
    fclose(file);
  }

  char command[512];
  snprintf(command, sizeof(command), "notepad \"%s\"", CONFIG_FILE);

  STARTUPINFO si = {0};
  PROCESS_INFORMATION pi = {0};
  si.cb = sizeof(si);

  if (CreateProcess(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si,
                    &pi)) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  } else {
    printf("Failed to open notepad.\n");
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    show_usage();
    return 0;
  }

  char *command = argv[1];

  if (strcmp(command, "reload") == 0 || strcmp(command, "install") == 0 ||
      strcmp(command, "uninstall") == 0) {
    if (!elevate_if_needed(argc, argv)) {
      return 0;
    }
  }

  if (strcmp(command, "help") == 0) {
    show_usage();
  } else if (strcmp(command, "enable") == 0) {
    enable_autolock();
  } else if (strcmp(command, "disable") == 0) {
    disable_autolock();
  } else if (strcmp(command, "reload") == 0) {
    reload_service();
  } else if (strcmp(command, "status") == 0) {
    show_status();
  } else if (strcmp(command, "config") == 0) {
    open_config();
  } else if (strcmp(command, "install") == 0) {
    install_service();
  } else if (strcmp(command, "uninstall") == 0) {
    uninstall_service();
  } else {
    printf("Unknown command: %s\n\n", command);
    show_usage();
    return 1;
  }

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--wait") == 0) {
      printf("\nPress Enter to close...");
      getchar();
      break;
    }
  }

  return 0;
}