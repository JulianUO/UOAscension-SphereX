# SphereServer X — Installation & Compilation Guide

This guide provides step-by-step instructions for installing dependencies, compiling **SphereServer X** (`Source-X`) from source code on Windows, Linux, and macOS, and deploying the server as a Linux systemd service via Debian packages.

---

## 1. Prerequisites & Dependencies

### 1.1 C++20 Compiler Requirements

SphereServer X is written in C++20. Supported compilers:
- **Windows**: Visual Studio 2019 version 16.11 or newer (MSVC), MinGW-w64 with GCC 8+
- **Linux**: GCC 8+ or Clang 10+
- **macOS**: Apple Clang 12+ or LLVM Clang 10+
- **Build Tool**: [CMake](https://cmake.org/) 3.16 or newer and [Git](https://git-scm.com/)

> [!IMPORTANT]
> Ensure Git is installed and available in your system `PATH` environment variable so CMake can query revision info.

---

### 1.2 Database Client Libraries

SphereServer X requires MariaDB / MySQL client headers and libraries:

#### Windows
- MariaDB Client library (`libmariadb.dll` v10.x package), bundled in `lib/bin/*architecture*/mariadb/libmariadb.dll`.

#### Linux (Debian / Ubuntu)
```bash
# Standard 64-bit packages
sudo apt update
sudo apt install git cmake build-essential libmariadb-dev mariadb-client

# 32-bit (x86) cross-compilation on 64-bit host
sudo apt install libmariadb-dev:i386 mariadb-client:i386
```

#### Linux (RHEL / CentOS / Fedora)
```bash
sudo dnf install git cmake gcc-c++ mariadb-connector-c mariadb-connector-c-devel
```

#### macOS (Homebrew)
```bash
brew install cmake mariadb-connector-c
```

---

## 2. Compiling from Source (CMake)

### 2.1 Standard Build Procedure (CLI)

1. Clone the repository and navigate to the project directory:
   ```bash
   git clone https://github.com/Sphereserver/Source-X.git
   cd Source-X
   ```

2. Create a build directory and configure CMake using a toolchain file from `cmake/toolchains/`:
   ```bash
   # Linux 64-bit Nightly Build example:
   mkdir build
   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Linux-GNU-x86_64.cmake \
         -G "Unix Makefiles" \
         -DCMAKE_BUILD_TYPE="Nightly" \
         -B ./build -S ./
   ```

3. Compile the server executable:
   ```bash
   cmake --build ./build --config Nightly
   ```

---

### 2.2 Visual Studio on Windows

1. Open **CMake GUI** or launch **Visual Studio** -> "Open a Local Folder".
2. Set source path to `Source-X` root and build path to `Source-X/build`.
3. Select Generator (e.g. `Visual Studio 17 2022`, x64).
4. Select Toolchain file `cmake/toolchains/Windows-MSVC.cmake`.
5. Click **Configure** -> **Generate** -> **Open Project**.
6. Build `SphereSvrX64` solution in **Nightly** or **Release** configuration.

---

### 2.3 Useful CMake Build Flags

| CMake Flag | Values | Description |
|------------|--------|-------------|
| `-DCMAKE_BUILD_TYPE` | `Release` / `Nightly` / `Debug` | Sets build optimization and diagnostic levels (`Nightly` recommended for production test shards). |
| `-DUSE_ASAN=ON` | `ON` / `OFF` | Enables AddressSanitizer for memory leak and illegal access detection (GCC/Clang/MSVC). |
| `-DUSE_UBSAN=ON` | `ON` / `OFF` | Enables UndefinedBehaviorSanitizer. |
| `-DBUILD_LOGIN_CRYPTO=ON` | `ON` / `OFF` | Builds native C++ `sphere_login_crypto` shared library for Python login server. |
| `-DUNIT_TESTING=ON` | `ON` / `OFF` | Enables C++ unit test executables. |

---

## 3. Debian / Ubuntu Package Installation (`packaging/`)

For production Linux deployments, SphereServer X can be packaged and run as a standard `systemd` daemon service.

### 3.1 Building the `.deb` Package

Requirements: `sudo apt install debhelper dpkg-dev`

```bash
cd packaging
# Generate versioned changelog
cat debian/data/changelog | sed -e "s/@version@/$(git rev-list --count HEAD)/" -e "s/@date@/$(date -R)/" > debian/changelog

# Build package
dpkg-buildpackage -us -uc -b
```

The resulting `.deb` package will be saved in the parent directory (e.g. `sphereserver_nightly_amd64.deb`).

---

### 3.2 Installing & Managing Service

1. Install the package:
   ```bash
   sudo chmod +x sphereserver_*.deb
   sudo apt install ./sphereserver_*.deb
   ```

2. Directory structure installed:
   - Executable: `/usr/bin/sphereserver`
   - Configuration: `/etc/sphereserver/sphere.ini`
   - Data & Scripts: `/opt/sphereserver/` (`mul/`, `scripts/`, `save/`)
   - Logs: `/var/log/sphereserver/`

3. Control systemd daemon:
   ```bash
   # Enable service on boot
   sudo systemctl enable sphereserver

   # Start / Stop / Restart
   sudo systemctl start sphereserver
   sudo systemctl stop sphereserver
   sudo systemctl status sphereserver

   # View live logs
   journalctl -u sphereserver -f
   ```

---

## 4. Next Steps

After compiling or installing:
- Read the **[Configuration Guide](configuration.md)** to set up `sphere.ini`, client MUL files, and database options.
- Read the **[Getting Started Tutorial](Getting-started.md)** for initial world setup and administrative commands.
