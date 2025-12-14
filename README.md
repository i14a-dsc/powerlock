<h3 align="center">🔒⚡<br/><img src="assets/transparent.png" height="30" width="0px"/>PowerLock<img src="assets/transparent.png" height="30" width="0px"/></h3>

<p align="center">
  <a href="https://github.com/i14a-dsc/powerlock/releases">
    <img src="https://img.shields.io/github/v/release/i14a-dsc/powerlock" alt="Latest Release">
  </a>
  <a href="https://github.com/i14a-dsc/powerlock/releases">
    <img src="https://img.shields.io/github/downloads/i14a-dsc/powerlock/total" alt="Downloads">
  </a>
</p>

## ✨ Features

- 🔋 Monitors AC adapter connection status in real-time
- 🔒 Automatically locks workstation when AC is disconnected
- ⚙️ Configuration file support with manual reload
- 🛡️ Smart first-run detection (no lock if starting on battery)
- 🔄 Service reload without reinstallation
- 🚀 Lightweight Windows service implementation
- 🛠️ Easy management with powerlock command utility

## 🚀 Getting Started

### Prerequisites

- Windows operating system
- Administrator privileges (for service installation)

### Download Pre-built Binaries

Download the latest release for your architecture:

- **[Latest Release](https://github.com/i14a-dsc/powerlock/releases/latest)**
  - `power-lock-service-x64.zip` - For 64-bit Windows
  - `power-lock-service-x86.zip` - For 32-bit Windows
  - `power-lock-service-arm64.zip` - For ARM64 Windows

### Building from Source

#### Prerequisites for Building

- Visual Studio 2022 (or Visual Studio Build Tools)

#### Building

1. Clone the repository:

```bash
git clone https://github.com/i14a-dsc/powerlock.git
cd powerlock
```

2. Build the project using the provided build scripts:

```bash
# Single architecture
build.bat          # x64 (default)
build-x64.bat       # x64
build-x86.bat       # x86
build-arm64.bat     # ARM64

# All architectures
build-all.bat
```

#### PowerShell Users

```bash
cmd /c "build.bat"
```

### Installation

Install as Windows service (run as Administrator):

```bash
power_lock_service.exe install
```

### Usage

#### Quick Management (Recommended)

```bash
# Enable auto-lock
powerlock.exe enable

# Disable auto-lock
powerlock.exe disable

# Reload service (restart to apply config changes)
powerlock.exe reload

# Check status
powerlock.exe status

# Edit config file
powerlock.exe config
```

#### Manual Service Management

```bash
# Install service
power_lock_service.exe install

# Start service
sc start PowerLockService

# Stop service
sc stop PowerLockService

# Restart service (reload config)
power_lock_service.exe reload

# Remove service
power_lock_service.exe uninstall

# Test in console mode
power_lock_service.exe console

# Show current config
power_lock_service.exe config
```

## ⚙️ Configuration

Config file: `C:\Data\power.cfg`

```ini
[Power]
AutoLock=true
```

- `AutoLock=true` - Enable automatic locking
- `AutoLock=false` - Disable automatic locking

**Note:** Configuration changes require service restart. Use `powerlock.exe reload` to apply changes.

## 🏗️ Project Structure

```
├── .github/workflows/       # GitHub Actions
│   └── release.yml         # Release workflow
├── src/                    # Source files
│   ├── power_lock_service.c # Main service implementation
│   └── powerlock.c         # Management utility
├── bin/                    # Build outputs (x64, created by build)
│   ├── x86/                # x86 build outputs
│   └── arm64/              # ARM64 build outputs
├── assets/                 # Assets (icons, images)
├── build.bat              # Build script (x64)
├── build-x64.bat          # x64 build script
├── build-x86.bat          # x86 build script
├── build-arm64.bat        # ARM64 build script
├── build-all.bat          # Build all architectures
└── README.md
```

## 🔧 How It Works

The service uses the Windows API to:

1. 🔍 Monitor system power status using `GetSystemPowerStatus()`
2. 📊 Detect transitions from AC power to battery power
3. 🔒 Call `LockWorkStation()` when AC power is disconnected
4. 🛡️ Skip locking on first run if starting on battery power

## 📋 Behavior

- **First run on battery**: No lock (prevents unexpected lock on startup)
- **AC to battery transition**: Locks workstation (if AutoLock=true)
- **Battery to AC transition**: No action
- **Config changes**: Require service restart to take effect

## 📦 Dependencies

- Windows API (windows.h)
- Power Management API (powrprof.h)
- Service Control Manager API (advapi32.lib)

## 🐛 Troubleshooting

- **Build fails**: Install Visual Studio Build Tools
- **Service won't start**: Run `power_lock_service.exe install` as Administrator
- **Not locking**: Check Windows power settings and ensure battery is detected
- **Config not applied**: Restart service with `powerlock.exe reload`

## 📄 License

This project is in the public domain.

<p align="center">Copyright &copy; 2025 <a href="https://github.com/i14a-dsc" target="_blank">Your Username</a></p>
