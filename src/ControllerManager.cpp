// ControllerManager.cpp - Xbox controller input implementation
#include "ControllerManager.h"
#include <string>

ControllerManager::ControllerManager() 
    : connected(false)
    , controllerIndex(0)
{
    ZeroMemory(&currentState, sizeof(XINPUT_STATE));
    ZeroMemory(&previousState, sizeof(XINPUT_STATE));
}

ControllerManager::~ControllerManager() {
    // No cleanup needed for XInput
}

bool ControllerManager::Initialize() {
    // Try to find the first connected controller
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        XINPUT_STATE state;
        if (XInputGetState(i, &state) == ERROR_SUCCESS) {
            controllerIndex = i;
            connected = true;
            currentState = state;
            previousState = state;
            return true;
        }
    }
    
    connected = false;
    return false;
}

void ControllerManager::Update() {
    if (!connected) {
        // Try to reconnect
        Initialize();
        return;
    }
    
    // Store previous state
    previousState = currentState;
    
    // Get current state
    DWORD result = XInputGetState(controllerIndex, &currentState);
    if (result != ERROR_SUCCESS) {
        connected = false;
        return;
    }
}

bool ControllerManager::IsButtonPressed(WORD button) {
    if (!connected) return false;
    
    // Button was pressed if it's down now but wasn't down before
    bool currentPressed = (currentState.Gamepad.wButtons & button) != 0;
    bool previousPressed = (previousState.Gamepad.wButtons & button) != 0;
    
    return currentPressed && !previousPressed;
}

bool ControllerManager::IsDPadPressed(int direction) {
    if (!connected) return false;
    
    WORD button = 0;
    switch (direction) {
        case 0: button = XINPUT_GAMEPAD_DPAD_UP; break;
        case 1: button = XINPUT_GAMEPAD_DPAD_RIGHT; break;
        case 2: button = XINPUT_GAMEPAD_DPAD_DOWN; break;
        case 3: button = XINPUT_GAMEPAD_DPAD_LEFT; break;
        default: return false;
    }
    
    return IsButtonPressed(button);
}

bool ControllerManager::IsLeftStickPressed(int direction) {
    if (!connected) return false;
    
    // Get current and previous stick positions
    int currentX = NormalizeStickInput(currentState.Gamepad.sThumbLX);
    int currentY = NormalizeStickInput(currentState.Gamepad.sThumbLY);
    int previousX = NormalizeStickInput(previousState.Gamepad.sThumbLX);
    int previousY = NormalizeStickInput(previousState.Gamepad.sThumbLY);
    
    // Check if stick was just moved in the specified direction
    // Note: XInput Y-axis is inverted (positive = up, negative = down)
    switch (direction) {
        case 0: return (currentY == 1 && previousY != 1);   // Up (positive Y in XInput)
        case 1: return (currentX == 1 && previousX != 1);   // Right
        case 2: return (currentY == -1 && previousY != -1); // Down (negative Y in XInput)
        case 3: return (currentX == -1 && previousX != -1); // Left
        default: return false;
    }
}

bool ControllerManager::IsRightStickPressed(int direction) {
    if (!connected) return false;
    
    // Get current and previous right stick positions
    int currentX = NormalizeStickInput(currentState.Gamepad.sThumbRX);
    int currentY = NormalizeStickInput(currentState.Gamepad.sThumbRY);
    int previousX = NormalizeStickInput(previousState.Gamepad.sThumbRX);
    int previousY = NormalizeStickInput(previousState.Gamepad.sThumbRY);
    
    // Check if stick was just moved in the specified direction
    // Note: XInput Y-axis is inverted (positive = up, negative = down)
    switch (direction) {
        case 0: return (currentY == 1 && previousY != 1);   // Up (positive Y in XInput)
        case 1: return (currentX == 1 && previousX != 1);   // Right
        case 2: return (currentY == -1 && previousY != -1); // Down (negative Y in XInput)
        case 3: return (currentX == -1 && previousX != -1); // Left
        default: return false;
    }
}

int ControllerManager::GetDPadX() {
    if (!connected) return 0;
    
    if (currentState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) return 1;
    if (currentState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) return -1;
    return 0;
}

int ControllerManager::GetDPadY() {
    if (!connected) return 0;
    
    if (currentState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) return -1;
    if (currentState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) return 1;
    return 0;
}

int ControllerManager::GetLeftStickX() {
    if (!connected) return 0;
    return NormalizeStickInput(currentState.Gamepad.sThumbLX);
}

int ControllerManager::GetLeftStickY() {
    if (!connected) return 0;
    return NormalizeStickInput(currentState.Gamepad.sThumbLY);
}

int ControllerManager::GetRightStickX() {
    if (!connected) return 0;
    return NormalizeStickInput(currentState.Gamepad.sThumbRX);
}

int ControllerManager::GetRightStickY() {
    if (!connected) return 0;
    return NormalizeStickInput(currentState.Gamepad.sThumbRY);
}

int ControllerManager::NormalizeStickInput(SHORT value) {
    // Apply deadzone
    if (abs(value) < STICK_DEADZONE) {
        return 0;
    }
    
    // Normalize to -1, 0, 1
    if (value > STICK_DEADZONE) return 1;
    if (value < -STICK_DEADZONE) return -1;
    return 0;
}