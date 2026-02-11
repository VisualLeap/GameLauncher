// GameLauncher.cpp - Main application entry point
#include "GameLauncher.h"
#include <iostream>
#include <shellscalingapi.h>

#pragma comment(lib, "shcore.lib")

int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/) {
    // Set DPI awareness as the very first thing
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    
    // Initialize COM for shortcut parsing
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        MessageBox(nullptr, L"Failed to initialize COM", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    GameLauncher launcher;
    
    if (!launcher.Initialize()) {
        CoUninitialize();
        return -1;
    }

    int result = launcher.Run();
    
    launcher.Shutdown();
    CoUninitialize();
    
    return result;
}