// WindowManager.h - Window management and display
#pragma once

#include <windows.h>
#include <memory>
#include <vector>
#include <map>
#include "DataModels.h"

class GridRenderer;
class TrayManager;
class ShortcutScanner;
class ControllerManager;

class WindowManager {
public:
    WindowManager();
    ~WindowManager();

    bool CreateMainWindow(HINSTANCE hInstance);
    void ShowWindow();
    void HideWindow();
    void ToggleVisibility();
    void RefreshGrid();
    void BringToForeground();
    
    HWND GetWindowHandle() const { return mainWindow; }
    bool IsVisible() const;
    bool HasFocus() const;
    
    void SetTrayManager(TrayManager* trayMgr) { trayManager = trayMgr; }
    void SetShortcutScanner(ShortcutScanner* scanner) { shortcutScanner = scanner; }
    
    void HandleControllerInput();       // Controller input processing
    
    void SaveWindowState();
    void LoadWindowState();

private:
    HWND mainWindow;
    std::unique_ptr<GridRenderer> gridRenderer;
    std::unique_ptr<ControllerManager> controllerManager;
    TrayManager* trayManager; // Non-owning pointer
    ShortcutScanner* shortcutScanner; // Non-owning pointer
    bool isDragging;
    std::vector<TabInfo> tabs; // Tab data
    int activeTabIndex; // Currently active tab
    int savedActiveTabIndex; // Saved active tab from INI file
    int scrollOffset; // Vertical scroll offset in pixels
    int selectedIconIndex; // Currently selected icon (unified for mouse and keyboard)
    int lastSelectedIconIndex; // Last selected icon before it was cleared (for resuming navigation)
    bool usingKeyboardNavigation; // Whether last selection was via keyboard
    
    // Persistent offscreen buffer for double buffering (to avoid memory fragmentation)
    HDC offscreenDC;
    HBITMAP offscreenBitmap;
    HBITMAP oldBitmap;
    void* offscreenBits;            // Direct access to bitmap pixels for alpha manipulation
    int offscreenWidth;
    int offscreenHeight;
    bool isResizing;                // Track if window is being resized
    
    // Cached tab buffer for performance
    HDC tabBufferDC;
    HBITMAP tabBufferBitmap;
    HBITMAP oldTabBitmap;
    void* tabBufferBits;
    int tabBufferWidth;
    int tabBufferHeight;
    bool tabBufferDirty;            // Track if tabs need redrawing
    
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleCommand(WPARAM wParam, LPARAM lParam);
    void HandleWindowDrag(UINT message, WPARAM wParam, LPARAM lParam);
    void HandleMouseMove(int x, int y);
    void HandleLeftClick(int x, int y);
    void HandleDoubleClick(int x, int y);
    void HandleTabClick(int x, int y);  // New method for tab clicks
    void HandleMouseWheel(int delta);   // New method for mouse wheel scrolling
    void HandleJoystickScroll(int delta); // New method for joystick scrolling (bypasses WHEEL_DELTA division)
    void HandleKeyDown(WPARAM wParam);  // New method for keyboard navigation
    void HandleControllerNavigation(int moveX, int moveY); // Helper for controller navigation
    void SetActiveTab(int tabIndex);    // New method to switch tabs
    void SetSelectedIcon(int iconIndex, bool fromKeyboard = false); // New method to set selected icon
    void LaunchSelectedIcon();          // New method to launch selected icon
    void EnsureSelectedIconVisible();   // New method to scroll selected icon into view
    void DrawTabs(HDC hdc, const RECT& clientRect);  // New method to draw tabs
    void LoadShortcuts();
    
    RECT GetTabBarRect(const RECT& clientRect);      // New method
    RECT GetGridRect(const RECT& clientRect);        // New method
    int GetTabAtPoint(POINT point, const RECT& clientRect);  // New method
    bool IsWindows11OrGreater();                     // Windows version check
    COLORREF GetTabColor(const std::wstring& tabName, bool isActive); // Get color for specific tab
    float GetDpiScaleFactor();                       // Get DPI scaling factor for this window
    
    // Helper methods to reduce code duplication
    std::wstring GetIniFilePath() const;             // Get path to launcher.ini
    int GetScaledIconSize() const;                   // Get icon size with scale applied
    int CalculateGridColumns(const RECT& gridRect) const; // Calculate number of grid columns
    RECT GetOptimizedGridRect(const RECT& gridRect, int cols, int itemWidth, int availableWidth) const; // Get optimized repaint rect
    RECT GetGridRelativeRect(const RECT& gridRect) const; // Get grid-relative rect for hit testing
    bool IsValidTabState() const;                    // Validate tab state before operations
    
    static const wchar_t* WINDOW_CLASS_NAME;
};