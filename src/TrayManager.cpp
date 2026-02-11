// TrayManager.cpp - System tray management implementation
#include "TrayManager.h"
#include "DataModels.h"
#include "resources/resource.h"

TrayManager::TrayManager() 
    : parentWindow(nullptr)
    , contextMenu(nullptr)
{
    ZeroMemory(&trayData, sizeof(trayData));
}

TrayManager::~TrayManager() {
    RemoveTrayIcon();
    if (contextMenu) {
        DestroyMenu(contextMenu);
    }
}

bool TrayManager::CreateTrayIcon(HWND parent, HINSTANCE hInstance) {
    this->parentWindow = parent;
    
    // Initialize tray icon data with newer version for better control
    trayData.cbSize = sizeof(NOTIFYICONDATA);
    trayData.hWnd = parentWindow;
    trayData.uID = TRAY_ICON_ID;
    trayData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    trayData.uCallbackMessage = WM_TRAY_ICON;
    trayData.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL)); // Use our custom icon
    wcscpy_s(trayData.szTip, L"Game Launcher");
    
    // Use older version to ensure it goes to overflow area by default
    // Newer versions tend to be more prominent
    trayData.uVersion = NOTIFYICON_VERSION;
    
    // Add icon to system tray
    if (!Shell_NotifyIcon(NIM_ADD, &trayData)) {
        return false;
    }
    
    // Set the version to the older one which tends to stay in overflow
    Shell_NotifyIcon(NIM_SETVERSION, &trayData);
    
    // Create context menu
    CreateContextMenu();
    
    return true;
}

void TrayManager::RemoveTrayIcon() {
    if (trayData.hWnd) {
        Shell_NotifyIcon(NIM_DELETE, &trayData);
        ZeroMemory(&trayData, sizeof(trayData));
    }
}

void TrayManager::ShowContextMenu(POINT cursorPos) {
    if (!contextMenu) {
        return;
    }
    
    // Set foreground window to ensure menu appears properly
    SetForegroundWindow(parentWindow);
    
    // Show context menu
    TrackPopupMenu(
        contextMenu,
        TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        cursorPos.x,
        cursorPos.y,
        0,
        parentWindow,
        nullptr
    );
    
    // Required for proper menu behavior
    PostMessage(parentWindow, WM_NULL, 0, 0);
}

void TrayManager::HandleTrayMessage(WPARAM wParam, LPARAM lParam) {
    if (wParam != TRAY_ICON_ID) {
        return;
    }
    
    switch (lParam) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            // Left click (any variant) - toggle window visibility
            // Use a timer to add a small delay to avoid potential issues
            SetTimer(parentWindow, 1, 50, nullptr); // 50ms delay
            break;
            
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP: {
            // Right click - show context menu
            POINT cursorPos;
            GetCursorPos(&cursorPos);
            ShowContextMenu(cursorPos);
            break;
        }
    }
}

void TrayManager::CreateContextMenu() {
    contextMenu = CreatePopupMenu();
    if (!contextMenu) {
        return;
    }
    
    // Add menu items
    AppendMenu(contextMenu, MF_STRING, ID_TRAY_SHOW, L"&Show");
    AppendMenu(contextMenu, MF_STRING, ID_TRAY_REFRESH, L"&Refresh");
    AppendMenu(contextMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(contextMenu, MF_STRING, ID_TRAY_EXIT, L"E&xit");
}