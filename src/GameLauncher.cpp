// GameLauncher.cpp - Main application entry point
#include "GameLauncher.h"
#include <iostream>
#include <shellscalingapi.h>

#pragma comment(lib, "shcore.lib")

int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/) {
    // Set DPI awareness as the very first thing
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    
    GameLauncher launcher;
    
    // Check for single instance
    if (!launcher.CheckSingleInstance()) {
        return false;
    }
    
    // Initialize launcher
    if (!launcher.Initialize()) {
        return -1;
    }

    // Run launcher
    int result = launcher.Run();
    
    launcher.Shutdown();
   
    return result;
}