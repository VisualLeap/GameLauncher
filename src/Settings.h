// Settings.h - Centralized configuration management
#pragma once

#include <windows.h>
#include <string>
#include <map>

class Settings {
public:
    static Settings& Instance() {
        static Settings instance;
        return instance;
    }
    
    // Delete copy/move
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
    
    // Load/Save
    void Load();
    void Save();
    
    // Window settings
    int GetWindowX() const { return windowX; }
    int GetWindowY() const { return windowY; }
    int GetWindowWidth() const { return windowWidth; }
    int GetWindowHeight() const { return windowHeight; }
    int GetActiveTab() const { return activeTab; }
    
    void SetWindowX(int x) { windowX = x; }
    void SetWindowY(int y) { windowY = y; }
    void SetWindowWidth(int w) { windowWidth = w; }
    void SetWindowHeight(int h) { windowHeight = h; }
    void SetActiveTab(int tab) { activeTab = tab; }
    
    // Color settings
    COLORREF GetTabActiveColor() const { return tabActiveColor; }
    COLORREF GetTabInactiveColor() const { return tabInactiveColor; }
    COLORREF GetTabColor(const std::wstring& tabName) const;
    
    void SetTabActiveColor(COLORREF color) { tabActiveColor = color; }
    void SetTabInactiveColor(COLORREF color) { tabInactiveColor = color; }
    void SetTabColor(const std::wstring& tabName, COLORREF color);
    
    // Display settings
    float GetIconScale() const { return iconScale; }
    int GetIconLabelFontSize() const { return iconLabelFontSize; }
    int GetTabFontSize() const { return tabFontSize; }
    int GetIconSpacingHorizontal() const { return iconSpacingHorizontal; }
    int GetIconSpacingVertical() const { return iconSpacingVertical; }
    int GetTabHeight() const { return tabHeight; }
    int GetIconVerticalPadding() const { return iconVerticalPadding; }
    
    void SetIconScale(float scale) { iconScale = scale; }
    void SetIconLabelFontSize(int size) { iconLabelFontSize = size; }
    void SetTabFontSize(int size) { tabFontSize = size; }
    void SetIconSpacingHorizontal(int spacing) { iconSpacingHorizontal = spacing; }
    void SetIconSpacingVertical(int spacing) { iconSpacingVertical = spacing; }
    void SetTabHeight(int height) { tabHeight = height; }
    void SetIconVerticalPadding(int padding) { iconVerticalPadding = padding; }
    
    // Scrolling settings
    int GetMouseScrollSpeed() const { return mouseScrollSpeed; }
    int GetJoystickScrollSpeed() const { return joystickScrollSpeed; }
    
    void SetMouseScrollSpeed(int speed) { mouseScrollSpeed = speed; }
    void SetJoystickScrollSpeed(int speed) { joystickScrollSpeed = speed; }
    
private:
    Settings();
    
    std::wstring GetIniPath() const;
    
    // Window
    int windowX = -32768;
    int windowY = -32768;
    int windowWidth = 800;
    int windowHeight = 600;
    int activeTab = 0;
    
    // Colors
    COLORREF tabActiveColor = RGB(19, 147, 98);
    COLORREF tabInactiveColor = RGB(70, 70, 77);
    std::map<std::wstring, COLORREF> tabSpecificColors;
    
    // Display
    float iconScale = 1.0f;
    int iconLabelFontSize = 36;
    int tabFontSize = 16;
    int iconSpacingHorizontal = 12;
    int iconSpacingVertical = 12;
    int tabHeight = 40;
    int iconVerticalPadding = 4;
    
    // Scrolling
    int mouseScrollSpeed = 60;
    int joystickScrollSpeed = 120;
};
