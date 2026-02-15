// GameLauncher.h - Main application class header
#pragma once

#include <windows.h>
#include <string>
#include <memory>
#include "DataModels.h"
#include "WindowManager.h"
#include "TrayManager.h"
#include "ShortcutScanner.h"

class GameLauncher {
public:
    GameLauncher();
    ~GameLauncher();

    bool Initialize();
    int Run();
    void Shutdown();
    void HandleSecondInstanceSignal();
    void HandleTrayMessage(WPARAM wParam, LPARAM lParam);

private:
    // Core components
    std::unique_ptr<WindowManager> windowManager;
    std::unique_ptr<TrayManager> trayManager;
    std::unique_ptr<ShortcutScanner> scanner;
    
    // Single instance management
    HANDLE singleInstanceMutex;
    HWND messageWindow;
    
    // Configuration
    LauncherConfig config;
    
    // Private methods
    bool CheckSingleInstance();
    void CreateMessageWindow();
    
    // Static window procedure for message window
    static LRESULT CALLBACK MessageWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static GameLauncher* instance; // For static callback access
    
    // Constants
    static const wchar_t* MUTEX_NAME;
    static const wchar_t* MESSAGE_WINDOW_CLASS;
    static const UINT WM_SHOW_WINDOW;
};