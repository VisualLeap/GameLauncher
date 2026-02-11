// ControllerManager.h - Xbox controller input handling
#pragma once
#include <windows.h>
#include <xinput.h>

#pragma comment(lib, "xinput.lib")

class ControllerManager {
public:
    ControllerManager();
    ~ControllerManager();
    
    // Initialize controller support
    bool Initialize();
    
    // Update controller state (call this regularly)
    void Update();
    
    // Check if specific buttons were just pressed (not held)
    bool IsButtonPressed(WORD button);
    
    // Get directional input - returns true only on initial press (not held)
    bool IsDPadPressed(int direction); // 0=up, 1=right, 2=down, 3=left
    bool IsLeftStickPressed(int direction); // 0=up, 1=right, 2=down, 3=left
    bool IsRightStickPressed(int direction); // 0=up, 1=right, 2=down, 3=left
    
    // Get directional input (-1, 0, 1 for each axis) - for continuous movement
    int GetDPadX();
    int GetDPadY();
    int GetLeftStickX();
    int GetLeftStickY();
    int GetRightStickX();
    int GetRightStickY();
    
    // Get raw analog stick values (for magnitude comparison)
    SHORT GetLeftStickRawX() const { return currentState.Gamepad.sThumbLX; }
    SHORT GetLeftStickRawY() const { return currentState.Gamepad.sThumbLY; }
    
    // Check if controller is connected
    bool IsConnected() const { return connected; }
    
private:
    XINPUT_STATE currentState;
    XINPUT_STATE previousState;
    bool connected;
    DWORD controllerIndex;
    
    // Deadzone for analog stick (reduced for better sensitivity)
    static const int STICK_DEADZONE = 4000;
    
    // Helper to normalize stick input to -1, 0, 1
    int NormalizeStickInput(SHORT value);
};