// TrayManager.h - System tray management
#pragma once

#include <windows.h>
#include <shellapi.h>

class TrayManager {
public:
    TrayManager();
    ~TrayManager();

    bool CreateTrayIcon(HWND parentWindow, HINSTANCE hInstance);
    void RemoveTrayIcon();
    void ShowContextMenu(POINT cursorPos);
    
    void HandleTrayMessage(WPARAM wParam, LPARAM lParam);

private:
    NOTIFYICONDATA trayData;
    HMENU contextMenu;
    HWND parentWindow;
    
    void CreateContextMenu();
    
    static const UINT TRAY_ICON_ID = 1001;
    static const UINT WM_TRAY_ICON = WM_USER + 1;
    
    // Menu command IDs
    static const UINT ID_TRAY_SHOW = 2001;
    static const UINT ID_TRAY_REFRESH = 2002;
    static const UINT ID_TRAY_EXIT = 2003;
    static const UINT ID_TRAY_TOGGLE = 2004;
};