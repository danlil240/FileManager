# Dolphin Lite (Qt6)

A production-focused, lightweight file manager inspired by KDE Dolphin. Built with Qt6 Widgets, async file operations, split view, archive support, and an embedded terminal.

## Features
- Places sidebar (Home, Desktop, Downloads, Root)
- Breadcrumb path bar with clickable segments
- Back/Forward/Up navigation
- Details + Icon view toggle
- Search filter per pane
- File operations: copy, move, rename, delete, create folder/file
- Drag & drop between panes
- Archive: zip compress, zip + tar.* extract
- Split view (F3)
- Embedded terminal with pane sync (F4)
- Non-blocking action logging to `~/.cache/dolphin-lite/actions.log`

## Dependencies (Ubuntu 22.04+)
```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config \
  qt6-base-dev qt6-base-dev-tools \
  libvte-2.91-dev libgtk-3-dev \
  zip unzip
```

Notes:
- Embedded terminal uses VTE (GTK3). For reliable embedding, run under X11:
  - Log out ? select ?Ubuntu on Xorg?, or
  - Use `QT_QPA_PLATFORM=xcb` when launching.

## Build
```bash
cmake -S . -B build
cmake --build build -j
```

## Run
```bash
./build/dolphin-lite
```

## Logging
Actions are recorded in JSONL format at:
```
~/.cache/dolphin-lite/actions.log
```

## Keyboard Shortcuts
- Copy/Cut/Paste: Ctrl+C / Ctrl+X / Ctrl+V
- Delete: Del
- Rename: F2
- Split view: F3
- Focus terminal: F4

## Optional CI (GitHub Actions)
```yaml
name: build
on: [push, pull_request]
jobs:
  build:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4
      - name: Install deps
        run: |
          sudo apt update
          sudo apt install -y build-essential cmake pkg-config \
            qt6-base-dev qt6-base-dev-tools libvte-2.91-dev libgtk-3-dev zip unzip
      - name: Configure
        run: cmake -S . -B build
      - name: Build
        run: cmake --build build -j
```

## Extension Points
Key classes for adding features:
- `MainWindow`: top-level UI and action wiring
- `FilePane`: navigation, view switching, history
- `FileView`: view wrapper with drag/drop hooks
- `FileOpsController`: async file operations
- `ArchiveController`: compress/extract via system tools
- `TerminalWidget`: embedded VTE terminal
- `ActionLogger`: async JSONL logging

## License
MIT. See `LICENSE`.
