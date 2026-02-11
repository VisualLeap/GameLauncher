// GameLauncher_impl.cpp - Main application implementation
#include "GameLauncher.h"
#include <iostream>
#include <shellscalingapi.h>

#pragma comment(lib, "shcore.lib")

// Set DPI awareness as early as possible
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Static member definitions
GameLauncher* GameLauncher::instance = nullptr;
const wchar_t* GameLauncher::MUTEX_NAME = L"GameLauncherSingleInstance";
const wchar_t* GameLauncher::MESSAGE_WINDOW_CLASS = L"GameLauncherMessageWindow";
const UINT GameLauncher::WM_SHOW_WINDOW = WM_USER + 1;

GameLauncher::GameLauncher() 
    : singleInstanceMutex(nullptr)
    , messageWindow(nullptr)
{
    instance = this;
    
    // Initialize components
    windowManager = std::make_unique<WindowManager>();
    trayManager = std::make_unique<TrayManager>();
    scanner = std::make_unique<ShortcutScanner>();
}

GameLauncher::~GameLauncher() {
    // Destructor should not call Shutdown() - it should be called explicitly by WinMain
    // This is just cleanup for any remaining resources
    instance = nullptr;
}

bool GameLauncher::Initialize() {
    // DPI awareness is now set in WinMain before this function is called
    
    // Check for single instance
    if (!CheckSingleInstance()) {
        return false;
    }
    
    // Create message window for inter-process communication
    CreateMessageWindow();
    
    // Load configuration
    LoadConfiguration();
    
    // Set shortcut folder to Data subfolder
    std::wstring dataFolder = L"Data";
    config.shortcutFolder = dataFolder;
    
    // Initialize scanner with folder
    if (!scanner->Initialize()) {
        return false;
    }
    
    if (!scanner->SetFolder(dataFolder)) {
        // Continue anyway - user can fix this later
    }
    
    // Connect shortcut scanner to window manager BEFORE creating window
    windowManager->SetShortcutScanner(scanner.get());
    
    // Create main window
    if (!windowManager->CreateMainWindow(GetModuleHandle(nullptr))) {
        return false;
    }
    
    // Create system tray icon
    if (!trayManager->CreateTrayIcon(windowManager->GetWindowHandle(), GetModuleHandle(nullptr))) {
        // Continue with normal window operation
    }
    
    // Connect tray manager to window manager
    windowManager->SetTrayManager(trayManager.get());
    
    // Show window initially
    windowManager->ShowWindow();
    
    return true;
}

int GameLauncher::Run() {
    // Main message loop with controller input support
    MSG msg = {};
    bool running = true;
    
    while (running) {
        // Check for Windows messages first
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else {
            // No Windows messages - check controller input if window is visible and has focus
            if (windowManager && windowManager->IsVisible() && windowManager->HasFocus()) {
                windowManager->HandleControllerInput();
            }
            
            // Small sleep to prevent excessive CPU usage
            Sleep(1);
        }
    }
    
    return static_cast<int>(msg.wParam);
}

void GameLauncher::Shutdown() {
    // Save configuration
    SaveConfiguration();
    
    // Clean up components in reverse order
    scanner.reset();
    trayManager.reset();
    windowManager.reset();
    
    // Clean up message window
    if (messageWindow) {
        DestroyWindow(messageWindow);
        messageWindow = nullptr;
    }
    
    // Clean up single instance mutex
    if (singleInstanceMutex) {
        ReleaseMutex(singleInstanceMutex);
        CloseHandle(singleInstanceMutex);
        singleInstanceMutex = nullptr;
    }
}

bool GameLauncher::CheckSingleInstance() {
    // Try to create named mutex
    singleInstanceMutex = CreateMutex(nullptr, TRUE, MUTEX_NAME);
    
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance is running - signal it to show window
        HWND existingWindow = FindWindow(MESSAGE_WINDOW_CLASS, nullptr);
        if (existingWindow) {
            PostMessage(existingWindow, WM_SHOW_WINDOW, 0, 0);
        }
        
        // Clean up and exit this instance
        if (singleInstanceMutex) {
            CloseHandle(singleInstanceMutex);
            singleInstanceMutex = nullptr;
        }
        
        return false;
    }
    
    // We are the first instance
    return true;
}

void GameLauncher::CreateMessageWindow() {
    // Register window class for message window
    WNDCLASS wc = {};
    wc.lpfnWndProc = MessageWindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = MESSAGE_WINDOW_CLASS;
    
    RegisterClass(&wc);
    
    // Create hidden message window
    messageWindow = CreateWindow(
        MESSAGE_WINDOW_CLASS,
        L"GameLauncherMessageWindow",
        0, // No style - hidden window
        0, 0, 0, 0,
        HWND_MESSAGE, // Message-only window
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
}

void GameLauncher::HandleSecondInstanceSignal() {
    // Show the main window if it exists and is hidden
    if (windowManager) {
        if (!windowManager->IsVisible()) {
            windowManager->ShowWindow();
        } else {
            windowManager->BringToForeground();
        }
    }
}

void GameLauncher::HandleTrayMessage(WPARAM wParam, LPARAM lParam) {
    if (trayManager) {
        trayManager->HandleTrayMessage(wParam, lParam);
    }
}

LRESULT CALLBACK GameLauncher::MessageWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_SHOW_WINDOW && instance) {
        instance->HandleSecondInstanceSignal();
        return 0;
    }
    
    // Handle tray messages
    if (uMsg == WM_USER + 1 && instance) {
        instance->HandleTrayMessage(wParam, lParam);
        return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void GameLauncher::LoadConfiguration() {
    // For now, use defaults
    // TODO: Load from registry or config file in future tasks
}

void GameLauncher::SaveConfiguration() {
    // For now, do nothing
    // TODO: Save to registry or config file in future tasks
}