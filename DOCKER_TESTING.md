# Testing with Docker on Windows

## Prerequisites

1. **Install Docker Desktop for Windows**
   - Download from: https://www.docker.com/products/docker-desktop/
   - Enable WSL2 backend during installation

2. **Install an X Server** (to display GUI from Docker)
   - **VcXsrv** (Recommended): https://sourceforge.net/projects/vcxsrv/
   - Or **Xming**: https://sourceforge.net/projects/xming/

## Setup Steps

### 1. Start X Server (VcXsrv)

1. Launch **XLaunch** from Start Menu
2. Select **Multiple windows**, Display number: **0**
3. Click **Next**
4. Select **Start no client**
5. Click **Next**
6. **IMPORTANT**: Check **"Disable access control"** (required for Docker)
7. Click **Finish**

### 2. Build Docker Image

Open PowerShell in the project directory:

```powershell
cd c:\Users\danli\Projects\FileManager
docker build -t dolphin-lite .
```

This will:
- Create an Ubuntu 22.04 container
- Install all dependencies (Qt6, VTE, GTK3, etc.)
- Build the project

### 3. Run the Application

```powershell
docker run -it --rm `
  -e DISPLAY=host.docker.internal:0 `
  -v ${PWD}:/app `
  dolphin-lite
```

Or use docker-compose:

```powershell
docker-compose up
```

## Rebuilding After Code Changes

If you modify the source code:

```powershell
# Rebuild inside container
docker run -it --rm -v ${PWD}:/app dolphin-lite bash -c "cmake --build build -j"

# Then run
docker run -it --rm -e DISPLAY=host.docker.internal:0 -v ${PWD}:/app dolphin-lite
```

Or rebuild the entire image:

```powershell
docker build -t dolphin-lite .
docker run -it --rm -e DISPLAY=host.docker.internal:0 -v ${PWD}:/app dolphin-lite
```

## Development Workflow

For active development, use an interactive shell:

```powershell
docker run -it --rm `
  -e DISPLAY=host.docker.internal:0 `
  -v ${PWD}:/app `
  dolphin-lite bash
```

Inside the container:
```bash
# Rebuild
cmake --build build -j

# Run
./build/dolphin-lite

# Or run with X11 explicitly
QT_QPA_PLATFORM=xcb ./build/dolphin-lite
```

## Troubleshooting

### GUI doesn't appear
- Ensure VcXsrv is running
- Check "Disable access control" is enabled in VcXsrv
- Verify DISPLAY variable: `echo $DISPLAY` should show `host.docker.internal:0`

### Build errors
- Clean build: `docker build --no-cache -t dolphin-lite .`
- Check Docker has enough resources (Settings → Resources → increase CPU/Memory)

### Permission issues with volumes
- Run Docker Desktop as Administrator
- Or adjust file sharing settings in Docker Desktop

## Testing Specific Features

```bash
# Test file operations
docker run -it --rm -e DISPLAY=host.docker.internal:0 -v ${PWD}:/app dolphin-lite

# Check logs (inside container)
docker run -it --rm -v ${PWD}:/app dolphin-lite bash -c "cat ~/.cache/dolphin-lite/actions.log"
```

## Alternative: WSL2 without Docker

If Docker is too heavy, you can use WSL2 directly:

```bash
# In WSL2 Ubuntu
sudo apt update
sudo apt install -y build-essential cmake pkg-config \
  qt6-base-dev qt6-base-dev-tools \
  libvte-2.91-dev libgtk-3-dev zip unzip

cmake -S . -B build
cmake --build build -j
./build/dolphin-lite
```

With WSLg (Windows 11), GUI apps work automatically. For Windows 10, you still need VcXsrv.
