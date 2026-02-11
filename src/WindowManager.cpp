// WindowManager.cpp - Window management implementation
#include "WindowManager.h"
#include "GridRenderer.h"
#include "TrayManager.h"
#include "ShortcutScanner.h"
#include "ControllerManager.h"
#include "DataModels.h"
#include "resources/resource.h"
#include <dwmapi.h>
#include <algorithm>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "msimg32.lib")  // For GradientFill

const wchar_t* WindowManager::WINDOW_CLASS_NAME = L"GameLauncherWindow";

WindowManager::WindowManager() 
    : mainWindow(nullptr)
    , gridRenderer(std::make_unique<GridRenderer>())
    , controllerManager(std::make_unique<ControllerManager>())
    , trayManager(nullptr)
    , shortcutScanner(nullptr)
    , isDragging(false)
    , dragStartPoint({0, 0})
    , activeTabIndex(0)
    , savedActiveTabIndex(0)
    , scrollOffset(0)
    , selectedIconIndex(-1)
    , lastSelectedIconIndex(-1)
    , usingKeyboardNavigation(false)
    , tabActiveColor(RGB(19, 147, 98))      // Default green (0x139362)
    , tabInactiveColor(RGB(70, 70, 77))     // Default gray
    , mouseScrollSpeed(60)                  // Default mouse scroll speed (pixels per notch)
    , joystickScrollSpeed(120)              // Default joystick scroll speed multiplier
{
}

WindowManager::~WindowManager() {
    if (mainWindow) {
        DestroyWindow(mainWindow);
    }
}

// Helper method to get INI file path
std::wstring WindowManager::GetIniFilePath() const {
    // Use current working directory
    // When launched normally, this is the exe directory
    // When debugging in VS, this is the configured working directory
    wchar_t currentDir[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, currentDir);
    return std::wstring(currentDir) + L"\\launcher.ini";
}

// Helper method to calculate grid columns
int WindowManager::CalculateGridColumns(const RECT& gridRect) const {
    int availableWidth = gridRect.right - gridRect.left;
    int physicalIconSize = DesignConstants::TARGET_ICON_SIZE_PIXELS;
    int itemWidth = physicalIconSize + DesignConstants::ICON_PADDING;
    return (availableWidth / itemWidth > 1) ? (availableWidth / itemWidth) : 1;
}

// Helper method to get optimized grid rect for repainting
RECT WindowManager::GetOptimizedGridRect(const RECT& gridRect, int cols, int itemWidth, int availableWidth) const {
    int totalGridWidth = cols * itemWidth - DesignConstants::ICON_PADDING;
    int startX = gridRect.left + (availableWidth - totalGridWidth) / 2;
    
    RECT optimizedGridRect = gridRect;
    optimizedGridRect.left = startX - DesignConstants::SELECTION_EFFECT_MARGIN;
    optimizedGridRect.right = min(gridRect.right, startX + totalGridWidth + DesignConstants::SELECTION_EFFECT_MARGIN);
    
    return optimizedGridRect;
}

// Helper method to get grid-relative rect for hit testing
RECT WindowManager::GetGridRelativeRect(const RECT& gridRect) const {
    return {0, 0, gridRect.right - gridRect.left, gridRect.bottom - gridRect.top};
}

// Helper method to validate tab state
bool WindowManager::IsValidTabState() const {
    return !tabs.empty() && 
           activeTabIndex >= 0 && 
           activeTabIndex < static_cast<int>(tabs.size()) &&
           !tabs[activeTabIndex].shortcuts.empty();
}

bool WindowManager::CreateMainWindow(HINSTANCE hInstance) {
    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // We'll draw our own background
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GAMELAUNCHER));
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));
    
    if (!RegisterClassEx(&wc)) {
        return false;
    }
    
    // Get screen dimensions for centering
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // Default window size and position
    int windowWidth = 800;
    int windowHeight = 600;
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;
    
    // Try to load saved window state
    std::wstring iniPath = GetIniFilePath();
    
    // Read values from INI file
    int savedX = GetPrivateProfileInt(L"Window", L"X", -1, iniPath.c_str());
    int savedY = GetPrivateProfileInt(L"Window", L"Y", -1, iniPath.c_str());
    int savedWidth = GetPrivateProfileInt(L"Window", L"Width", 800, iniPath.c_str());
    int savedHeight = GetPrivateProfileInt(L"Window", L"Height", 600, iniPath.c_str());
    int savedActiveTab = GetPrivateProfileInt(L"Window", L"ActiveTab", 0, iniPath.c_str());
    
    // Validate the saved position is on screen and values are reasonable
    if (savedX >= 0 && savedY >= 0 && savedX < screenWidth && savedY < screenHeight && 
        savedWidth > 200 && savedHeight > 150 && savedWidth <= screenWidth && savedHeight <= screenHeight) {
        // Use saved position and size
        x = savedX;
        y = savedY;
        windowWidth = savedWidth;
        windowHeight = savedHeight;
    }
    
    // Store the saved active tab index for later use
    savedActiveTabIndex = savedActiveTab;
    
    // Create borderless but resizable window with layered style for transparency
    mainWindow = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW,      // Enable layered window for transparency + hide from taskbar
        WINDOW_CLASS_NAME,
        L"Game Launcher",
        WS_POPUP | WS_THICKFRAME | WS_VISIBLE,  // Keep WS_VISIBLE
        x, y, windowWidth, windowHeight,
        nullptr, nullptr, hInstance, this
    );
    
    if (!mainWindow) {
        return false;
    }
    
    // Set window transparency using layered window attributes
    SetLayeredWindowAttributes(mainWindow, 0, DesignConstants::WINDOW_OPACITY, LWA_ALPHA);
    
    // Use DWM to create thin, modern resizable borders
    MARGINS margins = {1, 1, 1, 1};  // 1 pixel border on all sides
    DwmExtendFrameIntoClientArea(mainWindow, &margins);
    
    // Enable Windows 11 rounded corners (if supported)
    if (IsWindows11OrGreater()) {
        DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_ROUND;
        DwmSetWindowAttribute(mainWindow, DWMWA_WINDOW_CORNER_PREFERENCE, 
                             &cornerPreference, sizeof(cornerPreference));
    }
    
    // Load saved active tab index for use in LoadShortcuts
    LoadWindowState();
    
    // Load shortcuts initially
    LoadShortcuts();
    
    // Initialize controller support
    controllerManager->Initialize();
    
    // Save initial window state to create INI file
    SaveWindowState();
    
    return true;
}

bool WindowManager::IsWindows11OrGreater() {
    // Windows 11 is version 10.0.22000 or higher
    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 10;
    osvi.dwMinorVersion = 0;
    osvi.dwBuildNumber = 22000;
    
    DWORDLONG conditionMask = 0;
    VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditionMask, VER_BUILDNUMBER, VER_GREATER_EQUAL);
    
    return VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER, conditionMask);
}

void WindowManager::ShowWindow() {
    if (mainWindow) {
        // More forceful approach to showing the window
        ::ShowWindow(mainWindow, SW_HIDE);  // First hide it
        ::ShowWindow(mainWindow, SW_SHOW);  // Then show it
        
        // Bring to foreground and activate
        BringToForeground();
        
        // Update the window
        UpdateWindow(mainWindow);
        
        // Force a repaint
        InvalidateRect(mainWindow, nullptr, TRUE);
    }
}

void WindowManager::HideWindow() {
    if (mainWindow) {
        ::ShowWindow(mainWindow, SW_HIDE);
    }
}

void WindowManager::ToggleVisibility() {
    if (IsVisible()) {
        HideWindow();
    } else {
        ShowWindow();
    }
}

void WindowManager::RefreshGrid() {
    // Save current state
    int savedTabIndex = activeTabIndex;
    int savedIconIndex = selectedIconIndex;
    int savedScrollOffset = scrollOffset;
    bool savedKeyboardNav = usingKeyboardNavigation;
    
    // Reload shortcuts
    LoadShortcuts();
    
    // Restore state if still valid
    if (savedTabIndex >= 0 && savedTabIndex < static_cast<int>(tabs.size())) {
        activeTabIndex = savedTabIndex;
        
        // Restore selected icon if still valid
        if (!tabs[activeTabIndex].shortcuts.empty()) {
            int maxIconIndex = static_cast<int>(tabs[activeTabIndex].shortcuts.size()) - 1;
            if (savedIconIndex >= 0 && savedIconIndex <= maxIconIndex) {
                selectedIconIndex = savedIconIndex;
                usingKeyboardNavigation = savedKeyboardNav;
            }
            
            // Restore scroll position (will be clamped by repaint if needed)
            scrollOffset = savedScrollOffset;
        }
    }
    
    if (mainWindow) {
        // Use FALSE to avoid unnecessary background erasing
        InvalidateRect(mainWindow, nullptr, FALSE);
    }
}

void WindowManager::BringToForeground() {
    if (mainWindow) {
        // Restore if minimized
        if (IsIconic(mainWindow)) {
            ::ShowWindow(mainWindow, SW_RESTORE);
        }
        
        // Bring to foreground and activate
        SetForegroundWindow(mainWindow);
        BringWindowToTop(mainWindow);
        SetActiveWindow(mainWindow);
        SetFocus(mainWindow);
    }
}

bool WindowManager::IsVisible() const {
    return mainWindow && IsWindowVisible(mainWindow);
}

bool WindowManager::HasFocus() const {
    return mainWindow && (GetForegroundWindow() == mainWindow);
}

LRESULT CALLBACK WindowManager::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    WindowManager* pThis = nullptr;
    
    if (uMsg == WM_NCCREATE) {
        // Get the WindowManager pointer from CreateWindow
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<WindowManager*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<WindowManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (pThis) {
        return pThis->HandleMessage(hwnd, uMsg, wParam, lParam);
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT WindowManager::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            
            // Create off-screen buffer for double buffering
            int width = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;
            
            HDC hdcBuffer = CreateCompatibleDC(hdc);
            HBITMAP hbmBuffer = CreateCompatibleBitmap(hdc, width, height);
            HBITMAP hbmOld = (HBITMAP)SelectObject(hdcBuffer, hbmBuffer);
            
            // Draw everything to the buffer
            // Draw modern background
            DrawModernBackground(hdcBuffer, clientRect);
            
            // Draw tabs if we have any
            if (!tabs.empty()) {
                DrawTabs(hdcBuffer, clientRect);
                
                // Draw grid in the remaining area
                RECT gridRect = GetGridRect(clientRect);
                if (gridRenderer && activeTabIndex >= 0 && activeTabIndex < static_cast<int>(tabs.size())) {
                    // Set up clipping region to prevent icons from drawing over tabs
                    HRGN clipRegion = CreateRectRgn(gridRect.left, gridRect.top, gridRect.right, gridRect.bottom);
                    SelectClipRgn(hdcBuffer, clipRegion);
                    
                    gridRenderer->SetShortcuts(&tabs[activeTabIndex].shortcuts);
                    gridRenderer->SetScrollOffset(scrollOffset);
                    gridRenderer->SetSelectedIcon(selectedIconIndex);
                    gridRenderer->SetDpiScaleFactor(GetDpiScaleFactor()); // Set DPI scale factor
                    gridRenderer->Render(hdcBuffer, gridRect);
                    
                    // Restore clipping region
                    SelectClipRgn(hdcBuffer, nullptr);
                    DeleteObject(clipRegion);
                }
            } else {
                // Show "No shortcuts found" message
                SetTextColor(hdcBuffer, RGB(255, 255, 255));
                SetBkMode(hdcBuffer, TRANSPARENT);
                
                HFONT hFont = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
                HFONT hOldFont = (HFONT)SelectObject(hdcBuffer, hFont);
                
                std::wstring noShortcutsMsg = L"No shortcuts found in Data folder";
                DrawText(hdcBuffer, noShortcutsMsg.c_str(), -1, &clientRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                
                SelectObject(hdcBuffer, hOldFont);
                DeleteObject(hFont);
            }
            
            // Copy the buffer to the screen in one operation (eliminates flicker)
            BitBlt(hdc, 0, 0, width, height, hdcBuffer, 0, 0, SRCCOPY);
            
            // Clean up
            SelectObject(hdcBuffer, hbmOld);
            DeleteObject(hbmBuffer);
            DeleteDC(hdcBuffer);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_LBUTTONDOWN:
            HandleMouseMove(LOWORD(lParam), HIWORD(lParam));
            HandleTabClick(LOWORD(lParam), HIWORD(lParam));  // Check tab clicks first
            HandleLeftClick(LOWORD(lParam), HIWORD(lParam));
            HandleWindowDrag(uMsg, wParam, lParam);
            return 0;
            
        case WM_RBUTTONDOWN:
            // Right click - hide window
            // First, ensure we're not in a dragging state and release any mouse capture
            if (isDragging) {
                isDragging = false;
                ReleaseCapture();
            }
            HideWindow();
            return 0;
            
        case WM_MOUSEMOVE:
            HandleMouseMove(LOWORD(lParam), HIWORD(lParam));
            HandleWindowDrag(uMsg, wParam, lParam);
            return 0;
            
        case WM_LBUTTONUP:
            HandleWindowDrag(uMsg, wParam, lParam);
            return 0;
            
        case WM_LBUTTONDBLCLK:
            HandleDoubleClick(LOWORD(lParam), HIWORD(lParam));
            return 0;
            
        case WM_MOUSEWHEEL:
            HandleMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
            
        case WM_SIZE:
        case WM_MOVE:
            // Save window state when moved or resized
            SaveWindowState();
            if (uMsg == WM_SIZE) {
                // Reset scroll position and selection on window resize to keep things simple
                scrollOffset = 0;
                selectedIconIndex = -1;
                usingKeyboardNavigation = false;
                
                // Invalidate to redraw grid with new size
                InvalidateRect(mainWindow, nullptr, TRUE);
            }
            break;
            
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                HideWindow();
                return 0;
            } else {
                HandleKeyDown(wParam);
                return 0;
            }
            break;
            
        case WM_CLOSE:
            // Don't actually close, just hide to tray
            HideWindow();
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            
        case WM_USER + 1: // WM_TRAY_ICON
            // Forward tray messages to tray manager
            if (trayManager) {
                trayManager->HandleTrayMessage(wParam, lParam);
            }
            return 0;
            
        case WM_COMMAND:
            return HandleCommand(wParam, lParam);
            
        case WM_TIMER:
            if (wParam == 1) { // Tray icon timer
                KillTimer(hwnd, 1);
                ToggleVisibility();
                return 0;
            }
            break;
            
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    
    return 0;
}

LRESULT WindowManager::HandleCommand(WPARAM wParam, LPARAM /*lParam*/) {
    UINT commandId = LOWORD(wParam);
    
    switch (commandId) {
        case 2001: // ID_TRAY_SHOW
            ShowWindow();
            BringToForeground();
            return 0;
            
        case 2002: // ID_TRAY_REFRESH
            RefreshGrid();
            return 0;
            
        case 2003: // ID_TRAY_EXIT
            PostMessage(mainWindow, WM_DESTROY, 0, 0);
            return 0;
            
        case 2004: // ID_TRAY_TOGGLE
            ToggleVisibility();
            return 0;
    }
    
    return 0;
}

void WindowManager::HandleWindowDrag(UINT message, WPARAM /*wParam*/, LPARAM /*lParam*/) {
    static POINT dragStart;
    
    switch (message) {
        case WM_LBUTTONDOWN: {
            // Check if click is outside shortcut areas (for now, allow dragging anywhere)
            isDragging = true;
            GetCursorPos(&dragStart);
            SetCapture(mainWindow);
            break;
        }
        
        case WM_MOUSEMOVE: {
            if (isDragging) {
                POINT current;
                GetCursorPos(&current);
                RECT windowRect;
                GetWindowRect(mainWindow, &windowRect);
                
                int newX = windowRect.left + (current.x - dragStart.x);
                int newY = windowRect.top + (current.y - dragStart.y);
                SetWindowPos(mainWindow, nullptr, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                
                dragStart = current;
            }
            break;
        }
        
        case WM_LBUTTONUP: {
            if (isDragging) {
                isDragging = false;
                ReleaseCapture();
            }
            break;
        }
    }
}

void WindowManager::DrawGradientBackground(HDC hdc, const RECT& rect) {

    // Use Windows' built-in GradientFill for smooth, anti-aliased gradients
    TRIVERTEX vertex[2];
    GRADIENT_RECT gRect;
    
    // Define gradient colors (slightly lighter at top, darker at bottom)
    COLORREF topColor = RGB(20, 20, 25);        // Slightly lighter at top
    COLORREF bottomColor = RGB(160, 172, 156);  // Darker at bottom
    
    // Set up the gradient vertices
    vertex[0].x = rect.left;
    vertex[0].y = rect.top;
    vertex[0].Red = GetRValue(topColor) << 8;    // GradientFill uses 16-bit color values
    vertex[0].Green = GetGValue(topColor) << 8;
    vertex[0].Blue = GetBValue(topColor) << 8;
    vertex[0].Alpha = 0x0000;
    
    vertex[1].x = rect.right;
    vertex[1].y = rect.bottom;
    vertex[1].Red = GetRValue(bottomColor) << 8;
    vertex[1].Green = GetGValue(bottomColor) << 8;
    vertex[1].Blue = GetBValue(bottomColor) << 8;
    vertex[1].Alpha = 0x0000;
    
    gRect.UpperLeft = 0;
    gRect.LowerRight = 1;
    
    // Draw the gradient (GRADIENT_FILL_RECT_V for vertical gradient)
    GradientFill(hdc, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
}

void WindowManager::DrawModernBackground(HDC hdc, const RECT& rect) {
    
    // Simple solid background for layered window
    HBRUSH baseBrush = CreateSolidBrush(DesignConstants::BACKGROUND_COLOR);
    FillRect(hdc, &rect, baseBrush);
    DeleteObject(baseBrush);
    
    // Add a subtle gradient for depth
    DrawGradientBackground(hdc, rect);
    
    // Note: Border drawing removed for cleaner appearance
}

void WindowManager::LoadShortcuts() {
    if (!shortcutScanner) {
        return;
    }
    
    // Scan for tabs
    tabs = shortcutScanner->ScanTabs();
    
    // Set active tab to saved tab if valid, otherwise first tab
    // Only do this during initial load (when activeTabIndex is 0 and tabs were empty)
    if (!tabs.empty() && activeTabIndex == 0) {
        int tabToActivate = 0;
        if (savedActiveTabIndex >= 0 && savedActiveTabIndex < static_cast<int>(tabs.size())) {
            tabToActivate = savedActiveTabIndex;
        }
        activeTabIndex = tabToActivate;
        SetActiveTab(tabToActivate);
    }
}

void WindowManager::HandleMouseMove(int x, int y) {
    if (!gridRenderer || !IsValidTabState()) {
        return;
    }
    
    POINT mousePoint = {x, y};
    RECT clientRect;
    GetClientRect(mainWindow, &clientRect);
    
    // Only handle mouse move in grid area (not in tab area)
    RECT gridRect = GetGridRect(clientRect);
    if (!PtInRect(&gridRect, mousePoint)) {
        // Clear selection if mouse is outside grid area and we're in mouse mode
        if (selectedIconIndex != -1 && !usingKeyboardNavigation) {
            SetSelectedIcon(-1, false);
        }
        return;
    }
    
    // Adjust mouse point relative to grid area
    mousePoint.x -= gridRect.left;
    mousePoint.y -= gridRect.top;
    
    RECT gridRelativeRect = GetGridRelativeRect(gridRect);
    int hoveredIndex = gridRenderer->GetClickedShortcut(mousePoint, gridRelativeRect);
    
    // Always update selection based on mouse hover - this switches back to mouse mode
    if (hoveredIndex != selectedIconIndex) {
        SetSelectedIcon(hoveredIndex, false);
    }
}

void WindowManager::HandleLeftClick(int x, int y) {
    if (!gridRenderer || !IsValidTabState()) {
        return;
    }
    
    POINT clickPoint = {x, y};
    RECT clientRect;
    GetClientRect(mainWindow, &clientRect);
    
    // Only handle clicks in grid area (not in tab area)
    RECT gridRect = GetGridRect(clientRect);
    if (!PtInRect(&gridRect, clickPoint)) {
        return;
    }
    
    // Adjust click point relative to grid area
    clickPoint.x -= gridRect.left;
    clickPoint.y -= gridRect.top;
    
    RECT gridRelativeRect = GetGridRelativeRect(gridRect);
    int clickedIndex = gridRenderer->GetClickedShortcut(clickPoint, gridRelativeRect);
    
    if (clickedIndex >= 0 && clickedIndex < static_cast<int>(tabs[activeTabIndex].shortcuts.size())) {
        // Single click - confirm selection
        SetSelectedIcon(clickedIndex, false);
    }
}

void WindowManager::HandleDoubleClick(int x, int y) {
    if (!gridRenderer || !IsValidTabState()) {
        return;
    }
    
    POINT clickPoint = {x, y};
    RECT clientRect;
    GetClientRect(mainWindow, &clientRect);
    
    // Only handle clicks in grid area (not in tab area)
    RECT gridRect = GetGridRect(clientRect);
    if (!PtInRect(&gridRect, clickPoint)) {
        return;
    }
    
    // Adjust click point relative to grid area
    clickPoint.x -= gridRect.left;
    clickPoint.y -= gridRect.top;
    
    RECT gridRelativeRect = GetGridRelativeRect(gridRect);
    int clickedIndex = gridRenderer->GetClickedShortcut(clickPoint, gridRelativeRect);
    
    if (clickedIndex >= 0 && clickedIndex < static_cast<int>(tabs[activeTabIndex].shortcuts.size())) {
        // Double click - launch the shortcut
        SetSelectedIcon(clickedIndex, false);
        LaunchSelectedIcon();
    }
}

void WindowManager::HandleMouseWheel(int delta) {
    if (!IsValidTabState()) {
        return;
    }
    
    // Calculate scroll amount (negative delta means scroll down)
    int scrollDelta = -delta / WHEEL_DELTA * mouseScrollSpeed;
    
    // Get grid area to calculate maximum scroll
    RECT clientRect;
    GetClientRect(mainWindow, &clientRect);
    RECT gridRect = GetGridRect(clientRect);
    
    int availableWidth = gridRect.right - gridRect.left;
    int physicalIconSize = DesignConstants::TARGET_ICON_SIZE_PIXELS;
    int itemWidth = physicalIconSize + DesignConstants::ICON_PADDING;
    int cols = CalculateGridColumns(gridRect);
    
    // Calculate new scroll offset with bounds checking
    int newScrollOffset = scrollOffset + scrollDelta;
    
    // Only calculate max scroll if we need to clamp
    int rows = (static_cast<int>(tabs[activeTabIndex].shortcuts.size()) + cols - 1) / cols;
    int totalItemHeight = physicalIconSize + DesignConstants::LABEL_HEIGHT + DesignConstants::LABEL_SPACING;
    int totalContentHeight = rows * (totalItemHeight + DesignConstants::ICON_PADDING);
    int availableHeight = gridRect.bottom - gridRect.top;
    int maxScroll = max(0, totalContentHeight - availableHeight);
    
    int clampedScrollOffset = max(0, min(newScrollOffset, maxScroll));
    
    // Only invalidate and repaint if the scroll position actually changed
    if (clampedScrollOffset != scrollOffset) {
        scrollOffset = clampedScrollOffset;
        
        // Always update selection to first FULLY visible icon after scrolling
        // Calculate which row is at the top of the visible area
        int rowHeight = totalItemHeight + DesignConstants::ICON_PADDING;
        
        // If a row is partially cut off at the top, we want the next row
        // So we add (rowHeight - 1) before dividing to round up
        int firstFullyVisibleRow = (scrollOffset + rowHeight - 1) / rowHeight;
        int firstVisibleIconIndex = firstFullyVisibleRow * cols;
        
        // Clamp to valid icon range
        int totalIcons = static_cast<int>(tabs[activeTabIndex].shortcuts.size());
        firstVisibleIconIndex = max(0, min(firstVisibleIconIndex, totalIcons - 1));
        
        // Update selection to first visible icon and enable keyboard navigation mode
        selectedIconIndex = firstVisibleIconIndex;
        usingKeyboardNavigation = true;
        
        // Get optimized repaint rect
        RECT optimizedGridRect = GetOptimizedGridRect(gridRect, cols, itemWidth, availableWidth);
        
        // Redraw only the actual grid area
        InvalidateRect(mainWindow, &optimizedGridRect, FALSE);
    }
}

void WindowManager::HandleJoystickScroll(int delta) {
    if (!IsValidTabState()) {
        return;
    }
    
    // Joystick scroll delta is already in pixels (no WHEEL_DELTA division needed)
    int scrollDelta = delta;
    
    // Get grid area to calculate maximum scroll
    RECT clientRect;
    GetClientRect(mainWindow, &clientRect);
    RECT gridRect = GetGridRect(clientRect);
    
    int availableWidth = gridRect.right - gridRect.left;
    int physicalIconSize = DesignConstants::TARGET_ICON_SIZE_PIXELS;
    int itemWidth = physicalIconSize + DesignConstants::ICON_PADDING;
    int cols = CalculateGridColumns(gridRect);
    
    // Calculate new scroll offset with bounds checking
    int newScrollOffset = scrollOffset + scrollDelta;
    
    // Only calculate max scroll if we need to clamp
    int rows = (static_cast<int>(tabs[activeTabIndex].shortcuts.size()) + cols - 1) / cols;
    int totalItemHeight = physicalIconSize + DesignConstants::LABEL_HEIGHT + DesignConstants::LABEL_SPACING;
    int totalContentHeight = rows * (totalItemHeight + DesignConstants::ICON_PADDING);
    int availableHeight = gridRect.bottom - gridRect.top;
    int maxScroll = max(0, totalContentHeight - availableHeight);
    
    int clampedScrollOffset = max(0, min(newScrollOffset, maxScroll));
    
    // Only invalidate and repaint if the scroll position actually changed
    if (clampedScrollOffset != scrollOffset) {
        scrollOffset = clampedScrollOffset;
        
        // Always update selection to first FULLY visible icon after scrolling
        // Calculate which row is at the top of the visible area
        int rowHeight = totalItemHeight + DesignConstants::ICON_PADDING;
        
        // If a row is partially cut off at the top, we want the next row
        // So we add (rowHeight - 1) before dividing to round up
        int firstFullyVisibleRow = (scrollOffset + rowHeight - 1) / rowHeight;
        int firstVisibleIconIndex = firstFullyVisibleRow * cols;
        
        // Clamp to valid icon range
        int totalIcons = static_cast<int>(tabs[activeTabIndex].shortcuts.size());
        firstVisibleIconIndex = max(0, min(firstVisibleIconIndex, totalIcons - 1));
        
        // Update selection to first visible icon and enable keyboard navigation mode
        selectedIconIndex = firstVisibleIconIndex;
        usingKeyboardNavigation = true;
        
        // Get optimized repaint rect
        RECT optimizedGridRect = GetOptimizedGridRect(gridRect, cols, itemWidth, availableWidth);
        
        // Redraw only the actual grid area
        InvalidateRect(mainWindow, &optimizedGridRect, FALSE);
    }
}

void WindowManager::HandleKeyDown(WPARAM wParam) {
    if (!IsValidTabState()) {
        return;
    }
    
    // Handle Tab key first, regardless of selection state
    if (wParam == VK_TAB) {
        // Switch to next tab (with wraparound)
        int nextTab = (activeTabIndex + 1) % static_cast<int>(tabs.size());
        SetActiveTab(nextTab);
        return;
    }
    
    // Switch to keyboard navigation mode for other keys
    usingKeyboardNavigation = true;
    
    // If no icon is selected, try to resume from last selected position
    if (selectedIconIndex == -1) {
        // If we have a last selected icon, use it as the starting point
        if (lastSelectedIconIndex != -1 && lastSelectedIconIndex < static_cast<int>(tabs[activeTabIndex].shortcuts.size())) {
            selectedIconIndex = lastSelectedIconIndex;
            // Don't return - let the navigation logic below handle the movement
        } else {
            // No previous selection - select first fully visible icon
            RECT clientRect;
            GetClientRect(mainWindow, &clientRect);
            RECT gridRect = GetGridRect(clientRect);
            int cols = CalculateGridColumns(gridRect);
            
            int physicalIconSize = DesignConstants::TARGET_ICON_SIZE_PIXELS;
            int totalItemHeight = physicalIconSize + DesignConstants::LABEL_HEIGHT + DesignConstants::LABEL_SPACING;
            int rowHeight = totalItemHeight + DesignConstants::ICON_PADDING;
            
            // Calculate first fully visible row
            int firstFullyVisibleRow = (scrollOffset + rowHeight - 1) / rowHeight;
            int firstVisibleIconIndex = firstFullyVisibleRow * cols;
            
            // Clamp to valid range
            int totalIcons = static_cast<int>(tabs[activeTabIndex].shortcuts.size());
            firstVisibleIconIndex = max(0, min(firstVisibleIconIndex, totalIcons - 1));
            
            SetSelectedIcon(firstVisibleIconIndex, true);
            return;
        }
    }
    
    // Get grid layout information using DPI-aware sizes
    RECT clientRect;
    GetClientRect(mainWindow, &clientRect);
    RECT gridRect = GetGridRect(clientRect);
    
    int cols = CalculateGridColumns(gridRect);
    int totalIcons = static_cast<int>(tabs[activeTabIndex].shortcuts.size());
    
    int newSelectedIndex = selectedIconIndex;
    
    switch (wParam) {
        case VK_LEFT:
            if (selectedIconIndex > 0) {
                newSelectedIndex = selectedIconIndex - 1;
            }
            break;
            
        case VK_RIGHT:
            if (selectedIconIndex < totalIcons - 1) {
                newSelectedIndex = selectedIconIndex + 1;
            }
            break;
            
        case VK_UP:
            if (selectedIconIndex >= cols) {
                newSelectedIndex = selectedIconIndex - cols;
            }
            break;
            
        case VK_DOWN:
            if (selectedIconIndex + cols < totalIcons) {
                // There's an icon directly below
                newSelectedIndex = selectedIconIndex + cols;
            } else {
                // No icon directly below, but check if there's another row
                int currentRow = selectedIconIndex / cols;
                int nextRow = currentRow + 1;
                int nextRowStart = nextRow * cols;
                
                if (nextRowStart < totalIcons) {
                    // There is a next row, move to the last icon on that row
                    int nextRowEnd = min(nextRowStart + cols - 1, totalIcons - 1);
                    newSelectedIndex = nextRowEnd;
                }
            }
            break;
            
        case VK_RETURN:
            LaunchSelectedIcon();
            return;
    }
    
    // Update selection if it changed
    if (newSelectedIndex != selectedIconIndex) {
        SetSelectedIcon(newSelectedIndex, true);
    }
}

void WindowManager::SaveWindowState() {
    if (!mainWindow) return;
    
    RECT rect;
    GetWindowRect(mainWindow, &rect);
    
    std::wstring iniPath = GetIniFilePath();
    
    // Convert values to strings
    std::wstring x = std::to_wstring(rect.left);
    std::wstring y = std::to_wstring(rect.top);
    std::wstring width = std::to_wstring(rect.right - rect.left);
    std::wstring height = std::to_wstring(rect.bottom - rect.top);
    std::wstring activeTab = std::to_wstring(activeTabIndex);
    
    // Write to INI file
    WritePrivateProfileString(L"Window", L"X", x.c_str(), iniPath.c_str());
    WritePrivateProfileString(L"Window", L"Y", y.c_str(), iniPath.c_str());
    WritePrivateProfileString(L"Window", L"Width", width.c_str(), iniPath.c_str());
    WritePrivateProfileString(L"Window", L"Height", height.c_str(), iniPath.c_str());
    WritePrivateProfileString(L"Window", L"ActiveTab", activeTab.c_str(), iniPath.c_str());
    
    // Save tab colors as hex values
    // Convert COLORREF to hex format (0xRRGGBB)
    DWORD activeColorHex = (GetRValue(tabActiveColor) << 16) | (GetGValue(tabActiveColor) << 8) | GetBValue(tabActiveColor);
    DWORD inactiveColorHex = (GetRValue(tabInactiveColor) << 16) | (GetGValue(tabInactiveColor) << 8) | GetBValue(tabInactiveColor);
    
    // Format as hex strings
    wchar_t activeColorStr[16];
    wchar_t inactiveColorStr[16];
    swprintf_s(activeColorStr, L"0x%X", activeColorHex);
    swprintf_s(inactiveColorStr, L"0x%X", inactiveColorHex);
    
    WritePrivateProfileString(L"Colors", L"TabActiveColor", activeColorStr, iniPath.c_str());
    WritePrivateProfileString(L"Colors", L"TabInactiveColor", inactiveColorStr, iniPath.c_str());
    
    // Save scroll speeds to [Scrolling] section
    std::wstring mouseScrollStr = std::to_wstring(mouseScrollSpeed);
    std::wstring joystickScrollStr = std::to_wstring(joystickScrollSpeed);
    WritePrivateProfileString(L"Scrolling", L"MouseScrollSpeed", mouseScrollStr.c_str(), iniPath.c_str());
    WritePrivateProfileString(L"Scrolling", L"JoystickScrollSpeed", joystickScrollStr.c_str(), iniPath.c_str());
    
    // Save per-tab colors to [TabColors] section
    for (const auto& tabColorPair : tabSpecificColors) {
        const std::wstring& tabName = tabColorPair.first;
        COLORREF tabColor = tabColorPair.second;
        
        // Convert COLORREF to hex format
        DWORD tabColorHex = (GetRValue(tabColor) << 16) | (GetGValue(tabColor) << 8) | GetBValue(tabColor);
        wchar_t tabColorStr[16];
        swprintf_s(tabColorStr, L"0x%X", tabColorHex);
        
        WritePrivateProfileString(L"TabColors", tabName.c_str(), tabColorStr, iniPath.c_str());
    }
}

void WindowManager::LoadWindowState() {
    std::wstring iniPath = GetIniFilePath();
    
    // Read saved active tab index
    int savedActiveTab = GetPrivateProfileInt(L"Window", L"ActiveTab", 0, iniPath.c_str());
    
    // Load tab colors from INI file (with defaults)
    // Colors are stored as hex values (e.g., 0x139362 for green)
    DWORD activeColorHex = GetPrivateProfileInt(L"Colors", L"TabActiveColor", 0x139362, iniPath.c_str());
    DWORD inactiveColorHex = GetPrivateProfileInt(L"Colors", L"TabInactiveColor", 0x46464D, iniPath.c_str());
    
    // Convert hex values to COLORREF (RGB format)
    tabActiveColor = RGB((activeColorHex >> 16) & 0xFF, (activeColorHex >> 8) & 0xFF, activeColorHex & 0xFF);
    tabInactiveColor = RGB((inactiveColorHex >> 16) & 0xFF, (inactiveColorHex >> 8) & 0xFF, inactiveColorHex & 0xFF);
    
    // Load scroll speeds from INI file (with defaults)
    mouseScrollSpeed = GetPrivateProfileInt(L"Scrolling", L"MouseScrollSpeed", 60, iniPath.c_str());
    joystickScrollSpeed = GetPrivateProfileInt(L"Scrolling", L"JoystickScrollSpeed", 120, iniPath.c_str());
    
    // Load per-tab colors from [TabColors] section
    tabSpecificColors.clear();
    
    // Get all key names from the TabColors section
    wchar_t keyNames[4096] = {0};
    DWORD keyNamesSize = GetPrivateProfileString(L"TabColors", nullptr, L"", keyNames, 4096, iniPath.c_str());
    
    if (keyNamesSize > 0) {
        // Parse the key names (null-separated list)
        wchar_t* currentKey = keyNames;
        while (*currentKey) {
            std::wstring tabName = currentKey;
            
            // Get the color value for this tab
            wchar_t colorValue[32] = {0};
            GetPrivateProfileString(L"TabColors", tabName.c_str(), L"", colorValue, 32, iniPath.c_str());
            
            if (wcslen(colorValue) > 0) {
                // Parse hex color value (e.g., "0x139362" or "139362")
                DWORD colorHex = 0;
                if (wcsstr(colorValue, L"0x") == colorValue || wcsstr(colorValue, L"0X") == colorValue) {
                    colorHex = wcstoul(colorValue, nullptr, 16);
                } else {
                    colorHex = wcstoul(colorValue, nullptr, 16);
                }
                
                // Convert to COLORREF and store
                COLORREF tabColor = RGB((colorHex >> 16) & 0xFF, (colorHex >> 8) & 0xFF, colorHex & 0xFF);
                tabSpecificColors[tabName] = tabColor;
            }
            
            // Move to next key name
            currentKey += wcslen(currentKey) + 1;
        }
    }
    
    // Store the saved active tab index for later use
    savedActiveTabIndex = savedActiveTab;
}

void WindowManager::HandleTabClick(int x, int y) {
    if (tabs.empty()) {
        return;
    }
    
    POINT clickPoint = {x, y};
    RECT clientRect;
    GetClientRect(mainWindow, &clientRect);
    
    int clickedTab = GetTabAtPoint(clickPoint, clientRect);
    
    if (clickedTab >= 0 && clickedTab < static_cast<int>(tabs.size())) {
        SetActiveTab(clickedTab);
    }
}

void WindowManager::SetActiveTab(int tabIndex) {
    if (tabIndex < 0 || tabIndex >= static_cast<int>(tabs.size())) {
        return;
    }
    
    if (activeTabIndex == tabIndex) {
        return; // No change needed
    }
    
    activeTabIndex = tabIndex;
    
    // Reset scroll offset when switching tabs
    scrollOffset = 0;
    
    // Reset selection when switching tabs
    selectedIconIndex = -1;
    lastSelectedIconIndex = -1;  // Clear last selection from previous tab
    usingKeyboardNavigation = false;
    
    // Update grid renderer to point directly to the active tab's shortcuts
    if (gridRenderer && activeTabIndex < static_cast<int>(tabs.size())) {
        gridRenderer->SetShortcuts(&tabs[activeTabIndex].shortcuts);
    }
    
    // Save the new active tab to INI file
    SaveWindowState();
    
    // Redraw window - use FALSE to avoid erasing background unnecessarily
    if (mainWindow) {
        InvalidateRect(mainWindow, nullptr, FALSE);
    }
}

void WindowManager::DrawTabs(HDC hdc, const RECT& clientRect) {
    if (tabs.empty()) return;

    RECT tabBarRect = GetTabBarRect(clientRect);
    
    // Create memory DC for proper alpha blending with DWM
    HDC memDC = CreateCompatibleDC(hdc);
    if (!memDC) {
        return;
    }
    
    int width = tabBarRect.right - tabBarRect.left;
    int height = tabBarRect.bottom - tabBarRect.top;
    
    // Create 32-bit bitmap for proper alpha
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    
    if (!hBitmap) {
        DeleteDC(memDC);
        return;
    }
    
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);
    
    // Fill with opaque tab bar background
    DWORD* pixels = (DWORD*)pBits;
    DWORD tabBarColor = 0xFF2D2D32; // Opaque dark gray (ARGB format)
    
    for (int i = 0; i < width * height; i++) {
        pixels[i] = tabBarColor;
    }
    
    // Set up text rendering on memory DC
    SetTextColor(memDC, RGB(255, 255, 255));
    SetBkMode(memDC, TRANSPARENT);
    
    HFONT hFont = CreateFont(25, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(memDC, hFont);
    
    int tabWidth = width / static_cast<int>(tabs.size());
    
    for (size_t i = 0; i < tabs.size(); ++i) {
        RECT tabRect;
        tabRect.left = static_cast<int>(i) * tabWidth;
        tabRect.right = tabRect.left + tabWidth;
        tabRect.top = 0;
        tabRect.bottom = height;
        
        // Ensure the last tab extends to the full width
        if (i == tabs.size() - 1) {
            tabRect.right = width;
        }
        
        // Draw tab background with opaque colors using per-tab customizable colors
        bool isActiveTab = (static_cast<int>(i) == activeTabIndex);
        COLORREF baseColor = GetTabColor(tabs[i].name, isActiveTab);
        DWORD tabColor = 0xFF000000 | // Full opacity
                        (GetRValue(baseColor) << 16) | 
                        (GetGValue(baseColor) << 8) | 
                        GetBValue(baseColor);
        
        // Fill tab area with solid color
        for (int y = tabRect.top; y < tabRect.bottom; y++) {
            for (int x = tabRect.left; x < tabRect.right; x++) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    pixels[y * width + x] = tabColor;
                }
            }
        }
        
        // Draw tab border
        HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(100, 100, 107));
        HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
        
        // Draw border around the tab
        MoveToEx(memDC, tabRect.left, tabRect.top, nullptr);
        LineTo(memDC, tabRect.right, tabRect.top);
        LineTo(memDC, tabRect.right, tabRect.bottom);
        LineTo(memDC, tabRect.left, tabRect.bottom);
        LineTo(memDC, tabRect.left, tabRect.top);
        
        SelectObject(memDC, oldPen);
        DeleteObject(borderPen);
        
        // Draw tab text
        RECT textRect = tabRect;
        textRect.left += 8;   // Left padding
        textRect.right -= 8;  // Right padding
        textRect.top += 4;    // Top padding
        textRect.bottom -= 4; // Bottom padding
        
        DrawText(memDC, tabs[i].name.c_str(), -1, &textRect, 
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    
    SelectObject(memDC, hOldFont);
    DeleteObject(hFont);
    
    // Use BitBlt instead of AlphaBlend since tabs are fully opaque
    BitBlt(hdc, tabBarRect.left, tabBarRect.top, width, height,
           memDC, 0, 0, SRCCOPY);
    
    // Cleanup
    SelectObject(memDC, oldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(memDC);
}

RECT WindowManager::GetTabBarRect(const RECT& clientRect) {
    RECT tabBarRect = clientRect;
    tabBarRect.bottom = tabBarRect.top + DesignConstants::TAB_HEIGHT;
    return tabBarRect;
}

RECT WindowManager::GetGridRect(const RECT& clientRect) {
    RECT gridRect = clientRect;
    gridRect.top += DesignConstants::TAB_HEIGHT;
    
    // Apply equal margins on all sides (left, right, and additional top margin)
    // The top already has TAB_HEIGHT, so we add GRID_MARGIN to match lateral margins
    gridRect.top += DesignConstants::GRID_MARGIN;
    gridRect.left += DesignConstants::GRID_MARGIN;
    gridRect.right -= DesignConstants::GRID_MARGIN;
    gridRect.bottom -= DesignConstants::GRID_MARGIN;
    
    return gridRect;
}

int WindowManager::GetTabAtPoint(POINT point, const RECT& clientRect) {
    if (tabs.empty()) {
        return -1;
    }
    
    RECT tabBarRect = GetTabBarRect(clientRect);
    
    if (!PtInRect(&tabBarRect, point)) {
        return -1;
    }
    
    int tabWidth = (tabBarRect.right - tabBarRect.left) / static_cast<int>(tabs.size());
    int tabIndex = (point.x - tabBarRect.left) / tabWidth;
    
    if (tabIndex >= 0 && tabIndex < static_cast<int>(tabs.size())) {
        return tabIndex;
    }
    
    return -1;
}

COLORREF WindowManager::GetTabColor(const std::wstring& tabName, bool isActive) {
    if (!isActive) {
        return tabInactiveColor; // All inactive tabs use the same color
    }
    
    // Check if this tab has a specific color defined
    auto it = tabSpecificColors.find(tabName);
    if (it != tabSpecificColors.end()) {
        return it->second; // Use tab-specific color
    }
    
    // Fall back to default active color
    return tabActiveColor;
}

void WindowManager::SetSelectedIcon(int iconIndex, bool fromKeyboard) {
    if (tabs.empty() || activeTabIndex >= static_cast<int>(tabs.size())) {
        return;
    }
    
    auto& currentTabShortcuts = tabs[activeTabIndex].shortcuts;
    if (currentTabShortcuts.empty()) {
        return;
    }
    
    // Validate icon index (-1 is valid for no selection)
    if (iconIndex < -1 || iconIndex >= static_cast<int>(currentTabShortcuts.size())) {
        return;
    }
    
    int oldSelectedIndex = selectedIconIndex;
    
    // Track last selected icon before clearing (but not when setting to -1)
    if (selectedIconIndex != -1 && iconIndex == -1) {
        lastSelectedIconIndex = selectedIconIndex;
    } else if (iconIndex != -1) {
        lastSelectedIconIndex = iconIndex;
    }
    
    selectedIconIndex = iconIndex;
    usingKeyboardNavigation = fromKeyboard;
    
    // Ensure the selected icon is visible if using keyboard
    if (fromKeyboard && selectedIconIndex != -1) {
        EnsureSelectedIconVisible();
    }
    
    // Invalidate the old and new selected icons for redraw
    RECT clientRect;
    GetClientRect(mainWindow, &clientRect);
    RECT gridRect = GetGridRect(clientRect);
    
    if (oldSelectedIndex != -1 && gridRenderer) {
        RECT oldIconBounds = gridRenderer->GetIconBounds(oldSelectedIndex, gridRect);
        InvalidateRect(mainWindow, &oldIconBounds, FALSE);
    }
    
    if (selectedIconIndex != -1 && gridRenderer) {
        RECT newIconBounds = gridRenderer->GetIconBounds(selectedIconIndex, gridRect);
        InvalidateRect(mainWindow, &newIconBounds, FALSE);
    }
}

void WindowManager::LaunchSelectedIcon() {
    if (!IsValidTabState() || selectedIconIndex < 0 || 
        selectedIconIndex >= static_cast<int>(tabs[activeTabIndex].shortcuts.size())) {
        return;
    }
    
    // Launch the selected shortcut
    const auto& shortcut = tabs[activeTabIndex].shortcuts[selectedIconIndex];
    
    HINSTANCE result = ShellExecute(
        mainWindow,
        L"open",
        shortcut.targetPath.c_str(),
        shortcut.arguments.empty() ? nullptr : shortcut.arguments.c_str(),
        shortcut.workingDirectory.empty() ? nullptr : shortcut.workingDirectory.c_str(),
        SW_SHOWNORMAL
    );
    
    // Check if launch was successful
    if (reinterpret_cast<intptr_t>(result) > 32) {
        // Success - minimize to tray
        HideWindow();
    } else {
        // Launch failed - show error message
        std::wstring errorMsg = L"Failed to launch: " + shortcut.displayName;
        MessageBox(mainWindow, errorMsg.c_str(), L"Launch Error", MB_OK | MB_ICONERROR);
    }
}

void WindowManager::EnsureSelectedIconVisible() {
    if (!IsValidTabState() || selectedIconIndex < 0 || 
        selectedIconIndex >= static_cast<int>(tabs[activeTabIndex].shortcuts.size())) {
        return;
    }
    
    // Get grid layout information using DPI-aware sizes
    RECT clientRect;
    GetClientRect(mainWindow, &clientRect);
    RECT gridRect = GetGridRect(clientRect);
    
    int availableWidth = gridRect.right - gridRect.left;
    int physicalIconSize = DesignConstants::TARGET_ICON_SIZE_PIXELS;
    int itemWidth = physicalIconSize + DesignConstants::ICON_PADDING;
    int cols = CalculateGridColumns(gridRect);
    
    // Calculate the selected icon's position
    int row = selectedIconIndex / cols;
    int totalItemHeight = physicalIconSize + DesignConstants::LABEL_HEIGHT + DesignConstants::LABEL_SPACING;
    int itemHeight = totalItemHeight + DesignConstants::ICON_PADDING;
    
    int iconTop = row * itemHeight - scrollOffset;
    int iconBottom = iconTop + totalItemHeight;
    
    int viewportTop = 0;
    int viewportBottom = gridRect.bottom - gridRect.top;
    
    // Get optimized repaint rect
    RECT optimizedGridRect = GetOptimizedGridRect(gridRect, cols, itemWidth, availableWidth);
    
    // Check if icon is above the viewport
    if (iconTop < viewportTop) {
        scrollOffset = row * itemHeight;
        InvalidateRect(mainWindow, &optimizedGridRect, FALSE);
    }
    // Check if icon is below the viewport
    else if (iconBottom > viewportBottom) {
        scrollOffset = (row * itemHeight) - viewportBottom + totalItemHeight;
        
        // Ensure we don't scroll past the maximum
        int totalRows = (static_cast<int>(tabs[activeTabIndex].shortcuts.size()) + cols - 1) / cols;
        int totalContentHeight = totalRows * itemHeight;
        int maxScroll = max(0, totalContentHeight - viewportBottom);
        scrollOffset = min(scrollOffset, maxScroll);
        
        InvalidateRect(mainWindow, &optimizedGridRect, FALSE);
    }
}

float WindowManager::GetDpiScaleFactor() {
    if (!mainWindow) {
        return 1.0f;
    }
    
    // Get DPI for this window
    UINT dpi = GetDpiForWindow(mainWindow);
    
    // Calculate scale factor (96 DPI is the baseline)
    float scaleFactor = static_cast<float>(dpi) / 96.0f;
    
    return scaleFactor;
}

void WindowManager::HandleControllerInput() {
    if (!controllerManager) {
        return;
    }
    
    // Update controller state
    controllerManager->Update();
    
    if (!controllerManager->IsConnected()) {
        return;
    }
    
    // Handle B button - hide window
    if (controllerManager->IsButtonPressed(XINPUT_GAMEPAD_B)) {
        HideWindow();
        return;  // Exit after hiding window
    }
    
    // Handle A button - launch selected icon
    if (controllerManager->IsButtonPressed(XINPUT_GAMEPAD_A)) {
        LaunchSelectedIcon();
        return;  // Exit after launching
    }
    
    // Handle shoulder buttons - change tabs (don't return, allow scrolling)
    if (controllerManager->IsButtonPressed(XINPUT_GAMEPAD_LEFT_SHOULDER)) {
        if (!tabs.empty()) {
            int newTab = (activeTabIndex - 1 + static_cast<int>(tabs.size())) % static_cast<int>(tabs.size());
            SetActiveTab(newTab);
        }
    }
    
    if (controllerManager->IsButtonPressed(XINPUT_GAMEPAD_RIGHT_SHOULDER)) {
        if (!tabs.empty()) {
            int newTab = (activeTabIndex + 1) % static_cast<int>(tabs.size());
            SetActiveTab(newTab);
        }
    }
    
    // Handle navigation - D-pad or left stick (only on initial press)
    bool dpadLeft = controllerManager->IsDPadPressed(3);
    bool dpadRight = controllerManager->IsDPadPressed(1);
    bool dpadUp = controllerManager->IsDPadPressed(0);
    bool dpadDown = controllerManager->IsDPadPressed(2);
    bool stickLeft = controllerManager->IsLeftStickPressed(3);
    bool stickRight = controllerManager->IsLeftStickPressed(1);
    bool stickUp = controllerManager->IsLeftStickPressed(0);
    bool stickDown = controllerManager->IsLeftStickPressed(2);
    
    bool moveLeft = dpadLeft || stickLeft;
    bool moveRight = dpadRight || stickRight;
    bool moveUp = dpadUp || stickUp;
    bool moveDown = dpadDown || stickDown;
    
    if (moveLeft || moveRight || moveUp || moveDown) {
        int moveX = 0;
        int moveY = 0;
        
        // D-pad has priority - it's digital so no magnitude comparison needed
        bool usingDpad = dpadLeft || dpadRight || dpadUp || dpadDown;
        
        if (usingDpad) {
            // D-pad: take whatever directions are pressed (no priority)
            if (dpadLeft) moveX = -1;
            else if (dpadRight) moveX = 1;
            
            if (dpadUp) moveY = -1;
            else if (dpadDown) moveY = 1;
        } else {
            // Analog stick: use magnitude comparison
            SHORT rawX = controllerManager->GetLeftStickRawX();
            SHORT rawY = controllerManager->GetLeftStickRawY();
            
            if (abs(rawY) > abs(rawX)) {
                // Vertical movement is stronger
                if (stickUp) moveY = -1;
                else if (stickDown) moveY = 1;
            } else {
                // Horizontal movement is stronger (or equal)
                if (stickLeft) moveX = -1;
                else if (stickRight) moveX = 1;
            }
        }
        
        HandleControllerNavigation(moveX, moveY);
    }
    
    // Handle right stick scrolling (continuous while held)
    // Always check scrolling, even if other buttons were pressed
    int rightStickY = controllerManager->GetRightStickY();
    
    if (rightStickY != 0) {
        // Continuous scrolling based on stick position
        // XInput: positive Y = up, negative Y = down
        // Scroll direction: negative = scroll down (content moves up)
        // So we need to invert: -rightStickY
        int scrollDelta = -rightStickY * joystickScrollSpeed;
        HandleJoystickScroll(scrollDelta);
    }
}

void WindowManager::HandleControllerNavigation(int moveX, int moveY) {
    if (!IsValidTabState()) {
        return;
    }
    
    // Switch to controller navigation mode
    usingKeyboardNavigation = true;
    
    // If no icon is selected, try to resume from last selected position
    if (selectedIconIndex == -1) {
        // If we have a last selected icon, use it as the starting point
        if (lastSelectedIconIndex != -1 && lastSelectedIconIndex < static_cast<int>(tabs[activeTabIndex].shortcuts.size())) {
            selectedIconIndex = lastSelectedIconIndex;
            // Don't return - let the navigation logic below handle the movement
        } else {
            // No previous selection - select first fully visible icon
            RECT clientRect;
            GetClientRect(mainWindow, &clientRect);
            RECT gridRect = GetGridRect(clientRect);
            int cols = CalculateGridColumns(gridRect);
            
            int physicalIconSize = DesignConstants::TARGET_ICON_SIZE_PIXELS;
            int totalItemHeight = physicalIconSize + DesignConstants::LABEL_HEIGHT + DesignConstants::LABEL_SPACING;
            int rowHeight = totalItemHeight + DesignConstants::ICON_PADDING;
            
            // Calculate first fully visible row
            int firstFullyVisibleRow = (scrollOffset + rowHeight - 1) / rowHeight;
            int firstVisibleIconIndex = firstFullyVisibleRow * cols;
            
            // Clamp to valid range
            int totalIcons = static_cast<int>(tabs[activeTabIndex].shortcuts.size());
            firstVisibleIconIndex = max(0, min(firstVisibleIconIndex, totalIcons - 1));
            
            SetSelectedIcon(firstVisibleIconIndex, true);
            return;
        }
    }
    
    // Get grid layout information using DPI-aware sizes
    RECT clientRect;
    GetClientRect(mainWindow, &clientRect);
    RECT gridRect = GetGridRect(clientRect);
    
    int cols = CalculateGridColumns(gridRect);
    int totalIcons = static_cast<int>(tabs[activeTabIndex].shortcuts.size());
    
    int newSelectedIndex = selectedIconIndex;
    
    // Handle horizontal movement
    if (moveX == -1 && selectedIconIndex > 0) {
        newSelectedIndex = selectedIconIndex - 1;
    } else if (moveX == 1 && selectedIconIndex < totalIcons - 1) {
        newSelectedIndex = selectedIconIndex + 1;
    }
    
    // Handle vertical movement
    if (moveY == -1 && selectedIconIndex >= cols) {
        newSelectedIndex = selectedIconIndex - cols;
    } else if (moveY == 1) {
        if (selectedIconIndex + cols < totalIcons) {
            // There's an icon directly below
            newSelectedIndex = selectedIconIndex + cols;
        } else {
            // No icon directly below, but check if there's another row
            int currentRow = selectedIconIndex / cols;
            int nextRow = currentRow + 1;
            int nextRowStart = nextRow * cols;
            
            if (nextRowStart < totalIcons) {
                // There is a next row, move to the last icon on that row
                int nextRowEnd = min(nextRowStart + cols - 1, totalIcons - 1);
                newSelectedIndex = nextRowEnd;
            }
        }
    }
    
    // Update selection if it changed
    if (newSelectedIndex != selectedIconIndex) {
        SetSelectedIcon(newSelectedIndex, true);
    }
}