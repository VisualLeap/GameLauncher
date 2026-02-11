// DataModels.h - Core data structures
#pragma once

#include <windows.h>
#include <string>
#include <vector>

// Structure to hold shortcut information
struct ShortcutInfo {
    std::wstring displayName;      // Name to show in grid
    std::wstring targetPath;       // Executable path
    std::wstring arguments;        // Command line arguments
    std::wstring workingDirectory; // Working directory
    std::wstring iconPath;         // Icon file path
    int iconIndex;                 // Icon index in file
    HICON largeIcon;              // Cached 128x128 icon
    bool isValid;                 // Whether shortcut is functional
    
    // Constructor
    ShortcutInfo() 
        : iconIndex(0)
        , largeIcon(nullptr)
        , isValid(false) 
    {}
    
    // Destructor to clean up icon handle
    ~ShortcutInfo() {
        if (largeIcon) {
            DestroyIcon(largeIcon);
            largeIcon = nullptr;
        }
    }
    
    // Move constructor for efficient vector operations
    ShortcutInfo(ShortcutInfo&& other) noexcept
        : displayName(std::move(other.displayName))
        , targetPath(std::move(other.targetPath))
        , arguments(std::move(other.arguments))
        , workingDirectory(std::move(other.workingDirectory))
        , iconPath(std::move(other.iconPath))
        , iconIndex(other.iconIndex)
        , largeIcon(other.largeIcon)
        , isValid(other.isValid)
    {
        other.largeIcon = nullptr; // Transfer ownership
    }
    
    // Move assignment operator
    ShortcutInfo& operator=(ShortcutInfo&& other) noexcept {
        if (this != &other) {
            // Clean up existing icon
            if (largeIcon) {
                DestroyIcon(largeIcon);
            }
            
            // Move data
            displayName = std::move(other.displayName);
            targetPath = std::move(other.targetPath);
            arguments = std::move(other.arguments);
            workingDirectory = std::move(other.workingDirectory);
            iconPath = std::move(other.iconPath);
            iconIndex = other.iconIndex;
            largeIcon = other.largeIcon;
            isValid = other.isValid;
            
            other.largeIcon = nullptr; // Transfer ownership
        }
        return *this;
    }
    
    // Delete copy constructor and assignment to prevent accidental copying
    ShortcutInfo(const ShortcutInfo&) = delete;
    ShortcutInfo& operator=(const ShortcutInfo&) = delete;
};

// Structure to hold tab information
struct TabInfo {
    std::wstring name;                    // Tab display name (folder name)
    std::wstring folderPath;              // Full path to folder
    std::vector<ShortcutInfo> shortcuts;  // Shortcuts in this tab
    
    TabInfo() = default;
    
    // Move constructor
    TabInfo(TabInfo&& other) noexcept
        : name(std::move(other.name))
        , folderPath(std::move(other.folderPath))
        , shortcuts(std::move(other.shortcuts))
    {}
    
    // Move assignment
    TabInfo& operator=(TabInfo&& other) noexcept {
        if (this != &other) {
            name = std::move(other.name);
            folderPath = std::move(other.folderPath);
            shortcuts = std::move(other.shortcuts);
        }
        return *this;
    }
    
    // Delete copy operations
    TabInfo(const TabInfo&) = delete;
    TabInfo& operator=(const TabInfo&) = delete;
};

// Application configuration structure
struct LauncherConfig {
    std::wstring shortcutFolder;   // Input folder path
    bool startMinimized;           // Start in tray
    int windowWidth;               // Last window width
    int windowHeight;              // Last window height
    int windowPosX;                // Last window X position
    int windowPosY;                // Last window Y position
    
    // Constructor with defaults
    LauncherConfig()
        : startMinimized(false)
        , windowWidth(800)
        , windowHeight(600)
        , windowPosX(CW_USEDEFAULT)
        , windowPosY(CW_USEDEFAULT)
    {}
};

// Design constants for modern aesthetic
struct DesignConstants {
    static const COLORREF BACKGROUND_COLOR = RGB(28, 28, 30);      // Dark charcoal
    static const COLORREF ACCENT_COLOR = RGB(0, 122, 255);         // Modern blue
    static const COLORREF HOVER_COLOR = RGB(255, 255, 255);        // White highlight
    static const COLORREF ERROR_COLOR = RGB(255, 69, 58);          // Red for errors
    static const int HOVER_ANIMATION_DURATION = 150;               // Smooth transitions
    static const int TARGET_ICON_SIZE_PIXELS = 256;                // Target physical icon size in pixels
    static const int ICON_SIZE = 256;                              // Logical icon size (will be adjusted for DPI)
    static const int ICON_PADDING = 16;                            // Space between icons
    static const int GRID_MARGIN = 24;                             // Grid margins
    static const int WINDOW_OPACITY = 242;                         // ~86% opacity
    static const int TAB_HEIGHT = 40;                              // Tab bar height (reduced from 60)
    static const int TAB_PADDING = 12;                             // Tab padding
    static const COLORREF TAB_ACTIVE_COLOR = RGB(19, 147, 98);     // Active tab color (0x139362)
    static const COLORREF TAB_INACTIVE_COLOR = RGB(60, 60, 67);    // Inactive tab color
    static const COLORREF TAB_TEXT_COLOR = RGB(255, 255, 255);     // Tab text color
    static const int LABEL_HEIGHT = 57;                            // Icon label height (increased to accommodate 2-3 lines)
    static const int LABEL_SPACING = 8;                            // Spacing between icon and label
    static const int SELECTION_EFFECT_MARGIN = 10;                 // Margin for selection effects in repaint optimization
    static const int SELECTION_BORDER_PADDING = 4;                 // Padding for selection border
};