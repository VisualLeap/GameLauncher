// Settings.cpp - Centralized configuration implementation
#include "Settings.h"

Settings::Settings() {
}

std::wstring Settings::GetIniPath() const {
    wchar_t currentDir[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, currentDir);
    return std::wstring(currentDir) + L"\\launcher.ini";
}

void Settings::Load() {
    std::wstring iniPath = GetIniPath();
    
    // Window settings
    windowX = GetPrivateProfileInt(L"Window", L"X", -32768, iniPath.c_str());
    windowY = GetPrivateProfileInt(L"Window", L"Y", -32768, iniPath.c_str());
    windowWidth = GetPrivateProfileInt(L"Window", L"Width", 800, iniPath.c_str());
    windowHeight = GetPrivateProfileInt(L"Window", L"Height", 600, iniPath.c_str());
    activeTab = GetPrivateProfileInt(L"Window", L"ActiveTab", 0, iniPath.c_str());
    
    // Color settings
    DWORD activeColorHex = GetPrivateProfileInt(L"Colors", L"TabActiveColor", 0x139362, iniPath.c_str());
    DWORD inactiveColorHex = GetPrivateProfileInt(L"Colors", L"TabInactiveColor", 0x46464D, iniPath.c_str());
    tabActiveColor = RGB((activeColorHex >> 16) & 0xFF, (activeColorHex >> 8) & 0xFF, activeColorHex & 0xFF);
    tabInactiveColor = RGB((inactiveColorHex >> 16) & 0xFF, (inactiveColorHex >> 8) & 0xFF, inactiveColorHex & 0xFF);
    
    // Display settings
    wchar_t scaleBuffer[32] = {0};
    GetPrivateProfileString(L"Display", L"IconScale", L"1.0", scaleBuffer, 32, iniPath.c_str());
    float loadedScale = static_cast<float>(_wtof(scaleBuffer));
    iconScale = max(0.25f, min(2.0f, loadedScale));
    
    iconLabelFontSize = GetPrivateProfileInt(L"Display", L"IconLabelFontSize", 36, iniPath.c_str());
    iconLabelFontSize = max(8, min(72, iconLabelFontSize));
    
    tabFontSize = GetPrivateProfileInt(L"Display", L"TabFontSize", 16, iniPath.c_str());
    tabFontSize = max(8, min(50, tabFontSize));
    
    iconSpacingHorizontal = GetPrivateProfileInt(L"Display", L"IconSpacingHorizontal", 12, iniPath.c_str());
    iconSpacingHorizontal = max(0, min(100, iconSpacingHorizontal));
    
    iconSpacingVertical = GetPrivateProfileInt(L"Display", L"IconSpacingVertical", 12, iniPath.c_str());
    iconSpacingVertical = max(0, min(100, iconSpacingVertical));
    
    tabHeight = GetPrivateProfileInt(L"Display", L"TabHeight", 40, iniPath.c_str());
    tabHeight = max(20, min(100, tabHeight));
    
    iconVerticalPadding = GetPrivateProfileInt(L"Display", L"IconVerticalPadding", 4, iniPath.c_str());
    iconVerticalPadding = max(0, min(50, iconVerticalPadding));
    
    // Scrolling settings
    mouseScrollSpeed = GetPrivateProfileInt(L"Scrolling", L"MouseScrollSpeed", 60, iniPath.c_str());
    joystickScrollSpeed = GetPrivateProfileInt(L"Scrolling", L"JoystickScrollSpeed", 120, iniPath.c_str());
    
    // Tab-specific colors
    tabSpecificColors.clear();
    wchar_t keyNames[4096] = {0};
    DWORD keyNamesSize = GetPrivateProfileString(L"TabColors", nullptr, L"", keyNames, 4096, iniPath.c_str());
    
    if (keyNamesSize > 0) {
        wchar_t* currentKey = keyNames;
        while (*currentKey) {
            std::wstring tabName = currentKey;
            wchar_t colorValue[32] = {0};
            GetPrivateProfileString(L"TabColors", tabName.c_str(), L"", colorValue, 32, iniPath.c_str());
            
            if (wcslen(colorValue) > 0) {
                DWORD colorHex = wcstoul(colorValue, nullptr, 16);
                COLORREF tabColor = RGB((colorHex >> 16) & 0xFF, (colorHex >> 8) & 0xFF, colorHex & 0xFF);
                tabSpecificColors[tabName] = tabColor;
            }
            
            currentKey += wcslen(currentKey) + 1;
        }
    }
}

void Settings::Save() {
    std::wstring iniPath = GetIniPath();
    
    // Window settings
    WritePrivateProfileString(L"Window", L"X", std::to_wstring(windowX).c_str(), iniPath.c_str());
    WritePrivateProfileString(L"Window", L"Y", std::to_wstring(windowY).c_str(), iniPath.c_str());
    WritePrivateProfileString(L"Window", L"Width", std::to_wstring(windowWidth).c_str(), iniPath.c_str());
    WritePrivateProfileString(L"Window", L"Height", std::to_wstring(windowHeight).c_str(), iniPath.c_str());
    WritePrivateProfileString(L"Window", L"ActiveTab", std::to_wstring(activeTab).c_str(), iniPath.c_str());
    
    // Color settings
    DWORD activeColorHex = (GetRValue(tabActiveColor) << 16) | (GetGValue(tabActiveColor) << 8) | GetBValue(tabActiveColor);
    DWORD inactiveColorHex = (GetRValue(tabInactiveColor) << 16) | (GetGValue(tabInactiveColor) << 8) | GetBValue(tabInactiveColor);
    
    wchar_t activeColorStr[16];
    wchar_t inactiveColorStr[16];
    swprintf_s(activeColorStr, L"0x%X", activeColorHex);
    swprintf_s(inactiveColorStr, L"0x%X", inactiveColorHex);
    
    WritePrivateProfileString(L"Colors", L"TabActiveColor", activeColorStr, iniPath.c_str());
    WritePrivateProfileString(L"Colors", L"TabInactiveColor", inactiveColorStr, iniPath.c_str());
    
    // Display settings
    wchar_t iconScaleStr[32];
    swprintf_s(iconScaleStr, L"%.2f", iconScale);
    WritePrivateProfileString(L"Display", L"IconScale", iconScaleStr, iniPath.c_str());
    WritePrivateProfileString(L"Display", L"IconLabelFontSize", std::to_wstring(iconLabelFontSize).c_str(), iniPath.c_str());
    WritePrivateProfileString(L"Display", L"TabFontSize", std::to_wstring(tabFontSize).c_str(), iniPath.c_str());
    WritePrivateProfileString(L"Display", L"IconSpacingHorizontal", std::to_wstring(iconSpacingHorizontal).c_str(), iniPath.c_str());
    WritePrivateProfileString(L"Display", L"IconSpacingVertical", std::to_wstring(iconSpacingVertical).c_str(), iniPath.c_str());
    WritePrivateProfileString(L"Display", L"TabHeight", std::to_wstring(tabHeight).c_str(), iniPath.c_str());
    WritePrivateProfileString(L"Display", L"IconVerticalPadding", std::to_wstring(iconVerticalPadding).c_str(), iniPath.c_str());
    
    // Scrolling settings
    WritePrivateProfileString(L"Scrolling", L"MouseScrollSpeed", std::to_wstring(mouseScrollSpeed).c_str(), iniPath.c_str());
    WritePrivateProfileString(L"Scrolling", L"JoystickScrollSpeed", std::to_wstring(joystickScrollSpeed).c_str(), iniPath.c_str());
    
    // Tab-specific colors
    for (const auto& pair : tabSpecificColors) {
        DWORD tabColorHex = (GetRValue(pair.second) << 16) | (GetGValue(pair.second) << 8) | GetBValue(pair.second);
        wchar_t tabColorStr[16];
        swprintf_s(tabColorStr, L"0x%X", tabColorHex);
        WritePrivateProfileString(L"TabColors", pair.first.c_str(), tabColorStr, iniPath.c_str());
    }
}

COLORREF Settings::GetTabColor(const std::wstring& tabName) const {
    auto it = tabSpecificColors.find(tabName);
    if (it != tabSpecificColors.end()) {
        return it->second;
    }
    return tabActiveColor;
}

void Settings::SetTabColor(const std::wstring& tabName, COLORREF color) {
    tabSpecificColors[tabName] = color;
}
