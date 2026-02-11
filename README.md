# Game Launcher

A lightweight, modern C++ game/app launcher for Windows that displays shortcuts as a grid of large icons with tabbed organization and controller support.

## Features

- **Tabbed Interface**: Organize games by category with color-coded tabs
- **Grid Display**: Large 256x256 pixel icons in a responsive grid layout
- **Modern UI**: Borderless window with gradient background and smooth animations
- **Controller Support**: Full Xbox controller navigation and input
- **Keyboard Navigation**: Arrow keys, Enter, Tab for keyboard-only control
- **Mouse Support**: Click, double-click, and scroll wheel navigation
- **System Tray**: Minimize to tray with quick access menu
- **Single Instance**: Only one launcher runs at a time
- **Configurable**: INI file for colors, scroll speeds, and preferences
- **DPI Aware**: Proper scaling on high-DPI displays

## Building

### Prerequisites

- Visual Studio 2022 (or later) with C++ desktop development workload
- Windows 10 SDK (10.0.19041.0 or later)
- C++17 compiler support

### Build Instructions

**Using Visual Studio:**
1. Open `GameLauncher.sln`
2. Select Release or Debug configuration
3. Build → Build Solution (Ctrl+Shift+B)
4. Output: `.vs\Release\GameLauncher.exe` or `.vs\Debug\GameLauncher.exe`

**Using Command Line (MSBuild):**
```cmd
# Release build
msbuild GameLauncher.sln /p:Configuration=Release /p:Platform=x64

# Debug build
msbuild GameLauncher.sln /p:Configuration=Debug /p:Platform=x64
```

**Clean Build:**
All build outputs are stored in the `.vs\` folder, which is automatically excluded from version control.

## Usage

1. Place game shortcuts (`.lnk` files) in the `Data\` folder
2. Organize shortcuts into subfolders (each subfolder becomes a tab)
3. Run `GameLauncher.exe`
4. Use mouse, keyboard, or controller to navigate and launch games

### Controls

**Mouse:**
- Click to select icon
- Double-click to launch
- Scroll wheel to scroll
- Click tabs to switch categories
- Drag window to move

**Keyboard:**
- Arrow keys: Navigate icons
- Enter: Launch selected game
- Tab: Switch to next tab
- Escape: Minimize to tray

**Controller (Xbox):**
- D-pad / Left stick: Navigate icons
- A button: Launch selected game
- Right stick: Scroll up/down
- LB/RB: Switch tabs
- Back button: Minimize to tray

## Configuration

Settings are stored in `launcher.ini` (created automatically):

```ini
[Window]
ActiveTab=0                    # Last active tab index

[Colors]
TabActiveColor=1283938         # Active tab color (RGB)
TabInactiveColor=4605517       # Inactive tab color (RGB)

[TabColors]
Games=1283938                  # Per-tab custom colors
Emulators=255

[Scrolling]
MouseScrollSpeed=60            # Mouse wheel scroll speed (pixels)
JoystickScrollSpeed=120        # Controller scroll speed (pixels)
```

## Project Structure

```
GameLauncher/
├── src/
│   ├── GameLauncher.h/.cpp          # Application entry point
│   ├── GameLauncher_impl.cpp        # Main application logic
│   ├── DataModels.h                 # Data structures and constants
│   ├── WindowManager.h/.cpp         # Window and input management
│   ├── GridRenderer.h/.cpp          # Icon grid rendering
│   ├── TrayManager.h/.cpp           # System tray integration
│   ├── ShortcutScanner.h/.cpp       # Shortcut discovery
│   ├── ShortcutParser.h/.cpp        # .lnk file parsing
│   ├── IconExtractor.h/.cpp         # Icon extraction from executables
│   ├── ControllerManager.h/.cpp     # Xbox controller input
│   ├── GameLauncher.vcxproj         # Visual Studio project
│   ├── GameLauncher.exe.manifest    # DPI awareness manifest
│   └── resources/
│       ├── GameLauncher.rc          # Resource definitions
│       ├── resource.h               # Resource IDs
│       └── Launcher.ico             # Application icon
├── Data/                            # Game shortcuts folder
├── GameLauncher.sln                 # Visual Studio solution
└── README.md                        # This file
```

## Technical Details

### Architecture
- **Single-threaded**: Uses Windows message loop with controller polling
- **No external dependencies**: Pure Win32 API and Windows SDK
- **Minimal memory**: Caches icons but releases unused resources
- **DPI-aware**: Per-monitor DPI awareness v2

### Technologies
- **Win32 API**: Core window management
- **DWM (Desktop Window Manager)**: Modern borders and transparency
- **GDI+**: Icon rendering and alpha blending
- **XInput**: Xbox controller support
- **COM/Shell**: Shortcut parsing and icon extraction

### Design Constants
- Icon size: 256x256 pixels (physical)
- Icon padding: 32 pixels
- Tab height: 48 pixels
- Window opacity: 86% (220/255)
- Background: Dark gradient (RGB 28,28,30 to 18,18,20)

## Development

This project follows a specification-driven development methodology with clean, maintainable C++ code.

## Known Limitations

- Windows 10/11 only
- Xbox controllers only (XInput API)
- Border color remains white (limitation of layered windows)
- No touch input support

## License

This project is part of a specification-driven development process.

## Contributing

This is a personal project developed using AI-assisted specification-driven development. Feel free to fork and adapt for your own use.
