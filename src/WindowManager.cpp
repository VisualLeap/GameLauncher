// WindowManager.cpp - Window management implementation
#include "WindowManager.h"
#include "GridRenderer.h"
#include "TrayManager.h"
#include "ShortcutScanner.h"
#include "ControllerManager.h"
#include "DataModels.h"
#include "Settings.h"
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
    , activeTabIndex(0)
    , savedActiveTabIndex(0)
    , scrollOffset(0)
    , selectedIconIndex(-1)
    , lastSelectedIconIndex(-1)
    , usingKeyboardNavigation(false)
    , offscreenDC(nullptr)
    , offscreenBitmap(nullptr)
    , oldBitmap(nullptr)
    , offscreenBits(nullptr)
    , offscreenWidth(0)
    , offscreenHeight(0)
    , isResizing(false)
    , tabBufferDC(nullptr)
    , tabBufferBitmap(nullptr)
    , oldTabBitmap(nullptr)
    , tabBufferBits(nullptr)
    , tabBufferWidth(0)
    , tabBufferHeight(0)
    , tabBufferDirty(true)
{
}

WindowManager::~WindowManager() {
    // Clean up offscreen buffer
    if (offscreenDC) {
        if (oldBitmap) {
            SelectObject(offscreenDC, oldBitmap);
        }
        if (offscreenBitmap) {
            DeleteObject(offscreenBitmap);
        }
        DeleteDC(offscreenDC);
    }
    
    // Clean up tab buffer
    if (tabBufferDC) {
        if (oldTabBitmap) {
            SelectObject(tabBufferDC, oldTabBitmap);
        }
        if (tabBufferBitmap) {
            DeleteObject(tabBufferBitmap);
        }
        DeleteDC(tabBufferDC);
    }
    
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

// Helper method to get scaled icon size
int WindowManager::GetScaledIconSize() const {
    return static_cast<int>(DesignConstants::TARGET_ICON_SIZE_PIXELS * Settings::Instance().GetIconScale());
}

// Helper method to calculate grid columns
int WindowManager::CalculateGridColumns(const RECT& gridRect) const {
    int availableWidth = gridRect.right - gridRect.left;
    int physicalIconSize = GetScaledIconSize();
    int itemWidth = physicalIconSize + Settings::Instance().GetIconSpacingHorizontal();
    return (availableWidth / itemWidth > 1) ? (availableWidth / itemWidth) : 1;
}

// Helper method to get optimized grid rect for repainting
RECT WindowManager::GetOptimizedGridRect(const RECT& gridRect, int cols, int itemWidth, int availableWidth) const {
    int totalGridWidth = cols * itemWidth - Settings::Instance().GetIconSpacingHorizontal();
    int startX = gridRect.left + (availableWidth - totalGridWidth) / 2;
    
    RECT optimizedGridRect = gridRect;
    optimizedGridRect.left = startX - DesignConstants::SELECTION_BORDER_EXTENSION;
    optimizedGridRect.right = min(gridRect.right, startX + totalGridWidth + DesignConstants::SELECTION_BORDER_EXTENSION);
    optimizedGridRect.top -= DesignConstants::SELECTION_BORDER_EXTENSION;
    
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
    wc.hbrBackground = CreateSolidBrush(DesignConstants::BACKGROUND_COLOR); // Dark background to prevent white flash
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
    
    // Try to load saved window state from Settings
    Settings& settings = Settings::Instance();
    int savedX = settings.GetWindowX();
    int savedY = settings.GetWindowY();
    int savedWidth = settings.GetWindowWidth();
    int savedHeight = settings.GetWindowHeight();
    
    // Check if we have valid saved values (not the default sentinel value)
    bool hasValidSavedPosition = (savedX != -32768 && savedY != -32768);
    
    if (hasValidSavedPosition && savedWidth > 200 && savedHeight > 150) {
        // Get the monitor that contains the saved position
        POINT savedPos = {savedX + savedWidth / 2, savedY + savedHeight / 2};  // Use center point
        HMONITOR hMonitor = MonitorFromPoint(savedPos, MONITOR_DEFAULTTOPRIMARY);
        
        // Get monitor info to clamp window to monitor bounds
        MONITORINFO monitorInfo = {};
        monitorInfo.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(hMonitor, &monitorInfo);
        
        RECT workArea = monitorInfo.rcWork;  // Work area excludes taskbar
        int monitorWidth = workArea.right - workArea.left;
        int monitorHeight = workArea.bottom - workArea.top;
        
        // Clamp width and height to monitor size
        windowWidth = min(savedWidth, monitorWidth);
        windowHeight = min(savedHeight, monitorHeight);
        
        // Allow negative coordinates but ensure at least 100 pixels of the window is visible
        const int MIN_VISIBLE = 100;
        x = max(workArea.left - windowWidth + MIN_VISIBLE, min(savedX, workArea.right - MIN_VISIBLE));
        y = max(workArea.top - windowHeight + MIN_VISIBLE, min(savedY, workArea.bottom - MIN_VISIBLE));
    } else {
        // Use default centered position on primary monitor
        HMONITOR hMonitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO monitorInfo = {};
        monitorInfo.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(hMonitor, &monitorInfo);
        
        RECT workArea = monitorInfo.rcWork;
        int monitorWidth = workArea.right - workArea.left;
        int monitorHeight = workArea.bottom - workArea.top;
        
        x = workArea.left + (monitorWidth - windowWidth) / 2;
        y = workArea.top + (monitorHeight - windowHeight) / 2;
    }
    
    // Store the saved active tab index for later use
    savedActiveTabIndex = settings.GetActiveTab();
    
    // Create borderless but resizable window with layered style for transparency
    mainWindow = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW,      // Enable layered window for transparency + hide from taskbar
        WINDOW_CLASS_NAME,
        L"Game Launcher",
        WS_POPUP | WS_THICKFRAME,               // Remove WS_VISIBLE - will show after first paint
        x, y, windowWidth, windowHeight,
        nullptr, nullptr, hInstance, this
    );
    
    if (!mainWindow) {
        return false;
    }
    
    // Note: Not using SetLayeredWindowAttributes - we use UpdateLayeredWindow with per-pixel alpha instead
    
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
            
            // Get full window size for layered window bitmap
            RECT windowRect;
            GetWindowRect(hwnd, &windowRect);
            int windowWidth = windowRect.right - windowRect.left;
            int windowHeight = windowRect.bottom - windowRect.top;
            
            // Get client area for drawing content
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            
            // Create or resize offscreen buffer if needed (but not during active resize)
            if ((!offscreenDC || offscreenWidth != windowWidth || offscreenHeight != windowHeight) && !isResizing) {
                // Clean up old buffer if it exists
                if (offscreenDC) {
                    if (oldBitmap) {
                        SelectObject(offscreenDC, oldBitmap);
                    }
                    if (offscreenBitmap) {
                        DeleteObject(offscreenBitmap);
                    }
                    DeleteDC(offscreenDC);
                }
                
                // Create 32-bit ARGB DIB section for per-pixel alpha at full window size
                BITMAPINFO bmi = {};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = windowWidth;
                bmi.bmiHeader.biHeight = -windowHeight;  // Top-down DIB
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;     // 32-bit ARGB
                bmi.bmiHeader.biCompression = BI_RGB;
                
                offscreenDC = CreateCompatibleDC(hdc);
                offscreenBitmap = CreateDIBSection(offscreenDC, &bmi, DIB_RGB_COLORS, &offscreenBits, nullptr, 0);
                oldBitmap = (HBITMAP)SelectObject(offscreenDC, offscreenBitmap);
                offscreenWidth = windowWidth;
                offscreenHeight = windowHeight;
            }
            
            // Draw everything to the persistent buffer first (GDI doesn't set alpha channel)
            // Clear entire buffer to nearly transparent (alpha=1 for hit testing, visually transparent)
            if (offscreenBits) {
                // Fill with 0x01000000 (alpha=1, RGB=0)
                // Can't use memset since it fills bytes, not DWORDs
                DWORD* pixels = (DWORD*)offscreenBits;
                DWORD fillValue = 0x01000000;
                int totalPixels = offscreenWidth * offscreenHeight;
                
                // Use rep stosd via intrinsic for fast bulk fill
                __stosd((unsigned long*)pixels, fillValue, totalPixels);
            }
            
            // Draw tabs if we have any
            RECT tabRect = {0, 0, 0, 0};
            if (!tabs.empty()) {
                DrawTabs(offscreenDC, clientRect);
                tabRect = GetTabBarRect(clientRect);  // Get tab region for alpha fixing
                
                // Draw grid in the remaining area
                RECT gridRect = GetGridRect(clientRect);
                if (gridRenderer && activeTabIndex >= 0 && activeTabIndex < static_cast<int>(tabs.size())) {
                    // Set up clipping region to prevent icons from drawing over tabs
                    // Extend clip region upward to allow selection border to be visible
                    HRGN clipRegion = CreateRectRgn(gridRect.left, gridRect.top - DesignConstants::SELECTION_BORDER_EXTENSION, gridRect.right, gridRect.bottom);
                    SelectClipRgn(offscreenDC, clipRegion);
                    
                    gridRenderer->SetShortcuts(&tabs[activeTabIndex].shortcuts);
                    gridRenderer->SetScrollOffset(scrollOffset);
                    gridRenderer->SetSelectedIcon(selectedIconIndex);
                    gridRenderer->SetDpiScaleFactor(GetDpiScaleFactor());
                    gridRenderer->SetIconScale(Settings::Instance().GetIconScale());
                    gridRenderer->SetIconLabelFontSize(Settings::Instance().GetIconLabelFontSize());
                    gridRenderer->SetIconSpacingHorizontal(Settings::Instance().GetIconSpacingHorizontal());
                    gridRenderer->SetIconSpacingVertical(Settings::Instance().GetIconSpacingVertical());
                    gridRenderer->SetIconVerticalPadding(Settings::Instance().GetIconVerticalPadding());
                    gridRenderer->Render(offscreenDC, gridRect);
                    
                    // Restore clipping region
                    SelectClipRgn(offscreenDC, nullptr);
                    DeleteObject(clipRegion);
                }
            } else {
                // Show "No shortcuts found" message
                SetTextColor(offscreenDC, RGB(255, 255, 255));
                SetBkMode(offscreenDC, TRANSPARENT);
                
                HFONT hFont = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
                HFONT hOldFont = (HFONT)SelectObject(offscreenDC, hFont);
                
                std::wstring noShortcutsMsg = L"No shortcuts found in Data folder";
                DrawText(offscreenDC, noShortcutsMsg.c_str(), -1, &clientRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                
                SelectObject(offscreenDC, hOldFont);
                DeleteObject(hFont);
            }
            
            // Fix alpha channel after GDI drawing (GDI sets alpha to 0, we need proper alpha for visible elements)
            // CRITICAL: AC_SRC_ALPHA requires premultiplied alpha - RGB must be multiplied by alpha
            if (offscreenBits && offscreenDC) {
                DWORD* pixels = (DWORD*)offscreenBits;
                int bufferWidth = offscreenWidth;
                int bufferHeight = offscreenHeight;
                int bgLuminance = (28 + 28 + 30) / 3;
                
                int clientWidth = clientRect.right - clientRect.left;
                int clientHeight = clientRect.bottom - clientRect.top;
                
                // 1. Process tab region (simple: all pixels opaque)
                if (!tabs.empty()) {
                    int tabTop = max(0, tabRect.top);
                    int tabBottom = min(clientHeight, min(tabRect.bottom, bufferHeight));
                    int tabLeft = max(0, tabRect.left);
                    int tabRight = min(clientWidth, min(tabRect.right, bufferWidth));
                    
                    for (int y = tabTop; y < tabBottom; y++) {
                        int rowOffset = y * bufferWidth;
                        for (int x = tabLeft; x < tabRight; x++) {
                            int i = rowOffset + x;
                            DWORD pixel = pixels[i];
                            BYTE currentAlpha = (pixel >> 24) & 0xFF;
                            
                            if (currentAlpha == 0) {
                                // Set alpha=255 for all tab pixels
                                pixels[i] = (255 << 24) | (pixel & 0x00FFFFFF);
                            }
                        }
                    }
                }
                
                // 2. Process icon label regions (text, shadows, borders)
                if (gridRenderer && activeTabIndex >= 0 && activeTabIndex < static_cast<int>(tabs.size())) {
                    RECT gridRect = GetGridRect(clientRect);
                    
                    for (size_t iconIdx = 0; iconIdx < tabs[activeTabIndex].shortcuts.size(); iconIdx++) {
                        RECT iconBounds = gridRenderer->GetIconBounds(static_cast<int>(iconIdx), gridRect);
                        
                        int labelTop = max(0, min(iconBounds.top, clientHeight));
                        int labelBottom = max(0, min(iconBounds.bottom, clientHeight));
                        int labelLeft = max(0, min(iconBounds.left, clientWidth));
                        int labelRight = max(0, min(iconBounds.right, clientWidth));
                        
                        if (labelTop >= labelBottom || labelLeft >= labelRight) continue;
                        
                        for (int y = labelTop; y < labelBottom && y < bufferHeight; y++) {
                            int rowOffset = y * bufferWidth;
                            for (int x = labelLeft; x < labelRight && x < bufferWidth; x++) {
                                int i = rowOffset + x;
                                DWORD pixel = pixels[i];
                                BYTE currentAlpha = (pixel >> 24) & 0xFF;
                                
                                // Skip pixels with alpha already set (icons from AlphaBlend)
                                if (currentAlpha > 0) continue;
                                
                                BYTE r = (pixel >> 16) & 0xFF;
                                BYTE g = (pixel >> 8) & 0xFF;
                                BYTE b = pixel & 0xFF;
                                
                                // Check for selection border pixels (grey or white)
                                bool isGreyBorder = (r > 50 && r < 80 && g > 50 && g < 80 && b > 50 && b < 80);
                                bool isWhiteBorder = (r > 250 && g > 250 && b > 250);
                                if (isGreyBorder || isWhiteBorder) {
                                    pixels[i] = (255 << 24) | (r << 16) | (g << 8) | b;
                                    continue;
                                }
                                
                                // Skip pure black (undrawn background)
                                if (r == 0 && g == 0 && b == 0) continue;
                                
                                // Process text and shadow pixels
                                int luminance = (r + g + b) / 3;
                                bool isWhiteText = luminance > bgLuminance + 50;
                                bool isBlackShadow = luminance < 30;
                                
                                if (isWhiteText || isBlackShadow) {
                                    BYTE textAlpha;
                                    if (isBlackShadow) {
                                        textAlpha = (BYTE)min(255, (bgLuminance - luminance) * 255 / bgLuminance);
                                    } else {
                                        textAlpha = (BYTE)min(255, (luminance - bgLuminance) * 255 / (255 - bgLuminance));
                                    }
                                    
                                    // Premultiply RGB by alpha
                                    r = (r * textAlpha) / 255;
                                    g = (g * textAlpha) / 255;
                                    b = (b * textAlpha) / 255;
                                    pixels[i] = (textAlpha << 24) | (r << 16) | (g << 8) | b;
                                }
                            }
                        }
                    }
                }
                
                // Note: No need to clear margins or process background - everything defaults to transparent
                
                // Use UpdateLayeredWindow for per-pixel alpha compositing
                POINT ptSrc = {0, 0};
                SIZE sizeWnd = {offscreenWidth, offscreenHeight};
                BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
                UpdateLayeredWindow(hwnd, hdc, nullptr, &sizeWnd, offscreenDC, &ptSrc, 0, &blend, ULW_ALPHA);
            }
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_NCHITTEST: {
            // Handle hit testing for resize borders (transparent pixels block default hit testing)
            // Get mouse position in screen coordinates (lParam contains signed screen coords)
            POINT pt;
            pt.x = (short)LOWORD(lParam);  // Cast to short for signed values
            pt.y = (short)HIWORD(lParam);
            
            // Convert to client coordinates
            POINT clientPt = pt;
            ScreenToClient(hwnd, &clientPt);
            
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            
            const int BORDER_WIDTH = 16;  // Wider resize border for easier targeting
            
            // Check if mouse is within the window bounds (including border area)
            if (clientPt.x >= -BORDER_WIDTH && clientPt.x < clientRect.right + BORDER_WIDTH &&
                clientPt.y >= -BORDER_WIDTH && clientPt.y < clientRect.bottom + BORDER_WIDTH) {
                
                // Check if in resize border regions
                bool inLeft = clientPt.x < BORDER_WIDTH;
                bool inRight = clientPt.x >= clientRect.right - BORDER_WIDTH;
                bool inTop = clientPt.y < BORDER_WIDTH;
                bool inBottom = clientPt.y >= clientRect.bottom - BORDER_WIDTH;
                
                // Return appropriate hit test code for resize
                if (inLeft && inTop) return HTTOPLEFT;
                if (inRight && inTop) return HTTOPRIGHT;
                if (inLeft && inBottom) return HTBOTTOMLEFT;
                if (inRight && inBottom) return HTBOTTOMRIGHT;
                if (inLeft) return HTLEFT;
                if (inRight) return HTRIGHT;
                if (inTop) return HTTOP;
                if (inBottom) return HTBOTTOM;
            }
            
            // Fall back to default behavior
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
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
            
        case WM_ENTERSIZEMOVE:
            // User started resizing or moving the window
            isResizing = true;
            return 0;
            
        case WM_EXITSIZEMOVE:
            // User finished resizing or moving the window
            isResizing = false;
            // Save window state now that resize/move is complete
            SaveWindowState();
            // Force buffer recreation on next paint
            InvalidateRect(mainWindow, nullptr, FALSE);
            return 0;
            
        case WM_SIZE:
        case WM_MOVE:
            // Don't save during active resize - wait for WM_EXITSIZEMOVE
            if (!isResizing) {
                SaveWindowState();
            }
            if (uMsg == WM_SIZE) {
                // Reset scroll position and selection on window resize to keep things simple
                scrollOffset = 0;
                selectedIconIndex = -1;
                usingKeyboardNavigation = false;
                tabBufferDirty = true; // Mark tab buffer for redraw on resize
                
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
            // Save window state before destroying
            SaveWindowState();
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

void WindowManager::LoadShortcuts() {
    if (!shortcutScanner) {
        return;
    }
    
    // Scan for tabs
    tabs = shortcutScanner->ScanTabs();
    tabBufferDirty = true; // Mark tab buffer for redraw since tabs changed
    
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
    int scrollDelta = -delta / WHEEL_DELTA * Settings::Instance().GetMouseScrollSpeed();
    
    // Get grid area to calculate maximum scroll
    RECT clientRect;
    GetClientRect(mainWindow, &clientRect);
    RECT gridRect = GetGridRect(clientRect);
    
    int availableWidth = gridRect.right - gridRect.left;
    int physicalIconSize = GetScaledIconSize();
    int itemWidth = physicalIconSize + Settings::Instance().GetIconSpacingHorizontal();
    int cols = CalculateGridColumns(gridRect);
    
    // Calculate new scroll offset with bounds checking
    int newScrollOffset = scrollOffset + scrollDelta;
    
    // Only calculate max scroll if we need to clamp
    int rows = (static_cast<int>(tabs[activeTabIndex].shortcuts.size()) + cols - 1) / cols;
    int totalItemHeight = physicalIconSize + DesignConstants::LABEL_HEIGHT + Settings::Instance().GetIconVerticalPadding();
    int totalContentHeight = rows * (totalItemHeight + Settings::Instance().GetIconSpacingVertical());
    int availableHeight = gridRect.bottom - gridRect.top;
    int maxScroll = max(0, totalContentHeight - availableHeight);
    
    int clampedScrollOffset = max(0, min(newScrollOffset, maxScroll));
    
    // Only invalidate and repaint if the scroll position actually changed
    if (clampedScrollOffset != scrollOffset) {
        scrollOffset = clampedScrollOffset;
        
        // Always update selection to first FULLY visible icon after scrolling
        // Calculate which row is at the top of the visible area
        int rowHeight = totalItemHeight + Settings::Instance().GetIconSpacingVertical();
        
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
    int physicalIconSize = GetScaledIconSize();
    int itemWidth = physicalIconSize + Settings::Instance().GetIconSpacingHorizontal();
    int cols = CalculateGridColumns(gridRect);
    
    // Calculate new scroll offset with bounds checking
    int newScrollOffset = scrollOffset + scrollDelta;
    
    // Only calculate max scroll if we need to clamp
    int rows = (static_cast<int>(tabs[activeTabIndex].shortcuts.size()) + cols - 1) / cols;
    int totalItemHeight = physicalIconSize + DesignConstants::LABEL_HEIGHT + Settings::Instance().GetIconVerticalPadding();
    int totalContentHeight = rows * (totalItemHeight + Settings::Instance().GetIconSpacingVertical());
    int availableHeight = gridRect.bottom - gridRect.top;
    int maxScroll = max(0, totalContentHeight - availableHeight);
    
    int clampedScrollOffset = max(0, min(newScrollOffset, maxScroll));
    
    // Only invalidate and repaint if the scroll position actually changed
    if (clampedScrollOffset != scrollOffset) {
        scrollOffset = clampedScrollOffset;
        
        // Always update selection to first FULLY visible icon after scrolling
        // Calculate which row is at the top of the visible area
        int rowHeight = totalItemHeight + Settings::Instance().GetIconSpacingVertical();
        
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
            
            int physicalIconSize = GetScaledIconSize();
            int totalItemHeight = physicalIconSize + DesignConstants::LABEL_HEIGHT + Settings::Instance().GetIconVerticalPadding();
            int rowHeight = totalItemHeight + Settings::Instance().GetIconSpacingVertical();
            
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
    
    Settings& settings = Settings::Instance();
    settings.SetWindowX(rect.left);
    settings.SetWindowY(rect.top);
    settings.SetWindowWidth(rect.right - rect.left);
    settings.SetWindowHeight(rect.bottom - rect.top);
    settings.SetActiveTab(activeTabIndex);
    settings.Save();
}

void WindowManager::LoadWindowState() {
    Settings& settings = Settings::Instance();
    savedActiveTabIndex = settings.GetActiveTab();
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
    tabBufferDirty = true; // Mark tab buffer for redraw
    
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
    int width = tabBarRect.right - tabBarRect.left;
    int height = tabBarRect.bottom - tabBarRect.top;
    
    // Create or resize tab buffer if needed
    if (!tabBufferDC || tabBufferWidth != width || tabBufferHeight != height) {
        // Clean up old buffer
        if (tabBufferDC) {
            if (oldTabBitmap) {
                SelectObject(tabBufferDC, oldTabBitmap);
            }
            if (tabBufferBitmap) {
                DeleteObject(tabBufferBitmap);
            }
            DeleteDC(tabBufferDC);
        }
        
        // Create new buffer
        BITMAPINFO bmi = {0};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        
        tabBufferDC = CreateCompatibleDC(hdc);
        tabBufferBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &tabBufferBits, nullptr, 0);
        oldTabBitmap = (HBITMAP)SelectObject(tabBufferDC, tabBufferBitmap);
        tabBufferWidth = width;
        tabBufferHeight = height;
        tabBufferDirty = true;
    }
    
    // Render tabs to buffer if dirty
    if (tabBufferDirty) {
        DWORD* pixels = (DWORD*)tabBufferBits;
        DWORD tabBarColor = 0xFF2D2D32;
        
        // Fill background
        for (int i = 0; i < width * height; i++) {
            pixels[i] = tabBarColor;
        }
        
        // Set up text rendering
        SetTextColor(tabBufferDC, RGB(255, 255, 255));
        SetBkMode(tabBufferDC, TRANSPARENT);
        
        HFONT hFont = CreateFont(Settings::Instance().GetTabFontSize(), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(tabBufferDC, hFont);
        
        int tabWidth = width / static_cast<int>(tabs.size());
        
        for (size_t i = 0; i < tabs.size(); ++i) {
            RECT tabRect;
            tabRect.left = static_cast<int>(i) * tabWidth;
            tabRect.right = tabRect.left + tabWidth;
            tabRect.top = 0;
            tabRect.bottom = height;
            
            if (i == tabs.size() - 1) {
                tabRect.right = width;
            }
            
            // Draw tab background
            bool isActiveTab = (static_cast<int>(i) == activeTabIndex);
            COLORREF baseColor = GetTabColor(tabs[i].name, isActiveTab);
            DWORD tabColor = 0xFF000000 | (GetRValue(baseColor) << 16) | 
                            (GetGValue(baseColor) << 8) | GetBValue(baseColor);
            
            for (int y = tabRect.top; y < tabRect.bottom; y++) {
                for (int x = tabRect.left; x < tabRect.right; x++) {
                    if (x >= 0 && x < width && y >= 0 && y < height) {
                        pixels[y * width + x] = tabColor;
                    }
                }
            }
            
            // Draw tab border
            HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(100, 100, 107));
            HPEN oldPen = (HPEN)SelectObject(tabBufferDC, borderPen);
            
            MoveToEx(tabBufferDC, tabRect.left, tabRect.top, nullptr);
            LineTo(tabBufferDC, tabRect.right, tabRect.top);
            LineTo(tabBufferDC, tabRect.right, tabRect.bottom);
            LineTo(tabBufferDC, tabRect.left, tabRect.bottom);
            LineTo(tabBufferDC, tabRect.left, tabRect.top);
            
            SelectObject(tabBufferDC, oldPen);
            DeleteObject(borderPen);
            
            // Draw tab text
            RECT textRect = tabRect;
            textRect.left += 8;
            textRect.right -= 8;
            textRect.top += 4;
            textRect.bottom -= 4;
            
            DrawText(tabBufferDC, tabs[i].name.c_str(), -1, &textRect, 
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        
        SelectObject(tabBufferDC, hOldFont);
        DeleteObject(hFont);
        
        tabBufferDirty = false;
    }
    
    // BitBlt cached buffer to main buffer
    BitBlt(hdc, tabBarRect.left, tabBarRect.top, width, height,
           tabBufferDC, 0, 0, SRCCOPY);
}

RECT WindowManager::GetTabBarRect(const RECT& clientRect) {
    RECT tabBarRect = clientRect;
    tabBarRect.bottom = tabBarRect.top + Settings::Instance().GetTabHeight();
    return tabBarRect;
}

RECT WindowManager::GetGridRect(const RECT& clientRect) {
    RECT gridRect = clientRect;
    gridRect.top += Settings::Instance().GetTabHeight();
    
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
        return Settings::Instance().GetTabInactiveColor();
    }
    
    // Check if this tab has a specific color defined
    COLORREF tabColor = Settings::Instance().GetTabColor(tabName);
    return tabColor;
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
    
    // Sync grid renderer scroll offset before computing icon bounds
    if (gridRenderer) {
        gridRenderer->SetScrollOffset(scrollOffset);
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
    int physicalIconSize = GetScaledIconSize();
    int itemWidth = physicalIconSize + Settings::Instance().GetIconSpacingHorizontal();
    int cols = CalculateGridColumns(gridRect);
    
    // Calculate the selected icon's position
    int row = selectedIconIndex / cols;
    int totalItemHeight = physicalIconSize + DesignConstants::LABEL_HEIGHT + Settings::Instance().GetIconVerticalPadding();
    int itemHeight = totalItemHeight + Settings::Instance().GetIconSpacingVertical();
    
    // Account for the startY padding in GridRenderer (SELECTION_BORDER_PADDING)
    int iconTop = DesignConstants::SELECTION_BORDER_PADDING + row * itemHeight - scrollOffset;
    int iconBottom = iconTop + totalItemHeight;
    
    int viewportTop = 0;
    int viewportBottom = gridRect.bottom - gridRect.top;
    
    // Get optimized repaint rect
    RECT optimizedGridRect = GetOptimizedGridRect(gridRect, cols, itemWidth, availableWidth);
    
    // Check if icon is above the viewport
    if (iconTop - DesignConstants::SELECTION_BORDER_EXTENSION < viewportTop) {
        scrollOffset = max(0, DesignConstants::SELECTION_BORDER_PADDING + row * itemHeight - DesignConstants::SELECTION_BORDER_EXTENSION);
        InvalidateRect(mainWindow, &optimizedGridRect, FALSE);
    }
    // Check if icon is below the viewport
    else if (iconBottom > viewportBottom) {
        scrollOffset = DesignConstants::SELECTION_BORDER_PADDING + (row * itemHeight) - viewportBottom + totalItemHeight;
        
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
        int scrollDelta = -rightStickY * Settings::Instance().GetJoystickScrollSpeed();
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
            
            int physicalIconSize = GetScaledIconSize();
            int totalItemHeight = physicalIconSize + DesignConstants::LABEL_HEIGHT + Settings::Instance().GetIconVerticalPadding();
            int rowHeight = totalItemHeight + Settings::Instance().GetIconSpacingVertical();
            
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
