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
    HBITMAP iconBitmap;           // Cached 32-bit ARGB bitmap for alpha blending
    int iconBitmapWidth;          // Bitmap width
    int iconBitmapHeight;         // Bitmap height
    bool isValid;                 // Whether shortcut is functional
    
    // Constructor
    ShortcutInfo() 
        : iconIndex(0)
        , iconBitmap(nullptr)
        , iconBitmapWidth(0)
        , iconBitmapHeight(0)
        , isValid(false) 
    {}
    
    // Destructor to clean up bitmap handle
    ~ShortcutInfo() {
        if (iconBitmap) {
            DeleteObject(iconBitmap);
            iconBitmap = nullptr;
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
        , iconBitmap(other.iconBitmap)
        , iconBitmapWidth(other.iconBitmapWidth)
        , iconBitmapHeight(other.iconBitmapHeight)
        , isValid(other.isValid)
    {
        other.iconBitmap = nullptr; // Transfer ownership
    }
    
    // Move assignment operator
    ShortcutInfo& operator=(ShortcutInfo&& other) noexcept {
        if (this != &other) {
            // Clean up existing bitmap
            if (iconBitmap) {
                DeleteObject(iconBitmap);
            }
            
            // Move data
            displayName = std::move(other.displayName);
            targetPath = std::move(other.targetPath);
            arguments = std::move(other.arguments);
            workingDirectory = std::move(other.workingDirectory);
            iconPath = std::move(other.iconPath);
            iconIndex = other.iconIndex;
            iconBitmap = other.iconBitmap;
            iconBitmapWidth = other.iconBitmapWidth;
            iconBitmapHeight = other.iconBitmapHeight;
            isValid = other.isValid;
            
            other.iconBitmap = nullptr; // Transfer ownership
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
    static const int TARGET_ICON_SIZE_PIXELS = 256;                // Target physical icon size in pixels
    static const int ICON_PADDING = 30;                            // Space between icons
    static const int GRID_MARGIN = 24;                             // Grid margins
    static const int TAB_HEIGHT = 40;                              // Tab bar height
    static const int LABEL_HEIGHT = 70;                            // Icon label height
    static const int LABEL_SPACING = 8;                            // Spacing between icon and label
    static const int SELECTION_BORDER_INFLATE = 3;                 // InflateRect amount for selection border
    static const int SELECTION_BORDER_PEN_WIDTH = 4;               // Selection border pen width
    static const int SELECTION_BORDER_EXTENSION = SELECTION_BORDER_INFLATE + SELECTION_BORDER_PEN_WIDTH / 2; // Total extension above/below icon
    static const int SELECTION_BORDER_PADDING = 4;                 // Padding for selection border
};