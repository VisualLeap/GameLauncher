// Settings.cpp - Centralized configuration implementation
#include "Settings.h"

Settings::Settings() {
}

void Settings::Load(const std::wstring& path) {

    iniPath = path + L"\\launcher.ini";
    const wchar_t* iniPathPtr = iniPath.c_str();

    // Window settings
    windowX = GetPrivateProfileInt(L"Window", L"X", -32768, iniPathPtr);
    windowY = GetPrivateProfileInt(L"Window", L"Y", -32768, iniPathPtr);
    windowWidth = GetPrivateProfileInt(L"Window", L"Width", 800, iniPathPtr);
    windowHeight = GetPrivateProfileInt(L"Window", L"Height", 600, iniPathPtr);
    activeTab = GetPrivateProfileInt(L"Window", L"ActiveTab", 0, iniPathPtr);
    
    // Color settings
    DWORD activeColorHex = GetPrivateProfileInt(L"Colors", L"TabActiveColor", 0x139362, iniPathPtr);
    DWORD inactiveColorHex = GetPrivateProfileInt(L"Colors", L"TabInactiveColor", 0x46464D, iniPathPtr);
    tabActiveColor = RGB((activeColorHex >> 16) & 0xFF, (activeColorHex >> 8) & 0xFF, activeColorHex & 0xFF);
    tabInactiveColor = RGB((inactiveColorHex >> 16) & 0xFF, (inactiveColorHex >> 8) & 0xFF, inactiveColorHex & 0xFF);
    
    // Display settings
    wchar_t scaleBuffer[32] = {0};
    GetPrivateProfileString(L"Display", L"IconScale", L"1.0", scaleBuffer, 32, iniPathPtr);
    float loadedScale = static_cast<float>(_wtof(scaleBuffer));
    iconScale = max(0.25f, min(2.0f, loadedScale));
    
    iconLabelFontSize = GetPrivateProfileInt(L"Display", L"IconLabelFontSize", 36, iniPathPtr);
    iconLabelFontSize = max(8, min(72, iconLabelFontSize));
    
    tabFontSize = GetPrivateProfileInt(L"Display", L"TabFontSize", 16, iniPathPtr);
    tabFontSize = max(8, min(50, tabFontSize));
    
    iconSpacingHorizontal = GetPrivateProfileInt(L"Display", L"IconSpacingHorizontal", 12, iniPathPtr);
    iconSpacingHorizontal = max(0, min(100, iconSpacingHorizontal));
    
    iconSpacingVertical = GetPrivateProfileInt(L"Display", L"IconSpacingVertical", 12, iniPathPtr);
    iconSpacingVertical = max(0, min(100, iconSpacingVertical));
    
    tabHeight = GetPrivateProfileInt(L"Display", L"TabHeight", 40, iniPathPtr);
    tabHeight = max(20, min(100, tabHeight));
    
    iconVerticalPadding = GetPrivateProfileInt(L"Display", L"IconVerticalPadding", 4, iniPathPtr);
    iconVerticalPadding = max(0, min(50, iconVerticalPadding));
    
    // Scrolling settings
    mouseScrollSpeed = GetPrivateProfileInt(L"Scrolling", L"MouseScrollSpeed", 60, iniPathPtr);
    joystickScrollSpeed = GetPrivateProfileInt(L"Scrolling", L"JoystickScrollSpeed", 120, iniPathPtr);
    
    // Tab-specific colors
    tabSpecificColors.clear();
    wchar_t keyNames[4096] = {0};
    DWORD keyNamesSize = GetPrivateProfileString(L"TabColors", nullptr, L"", keyNames, 4096, iniPathPtr);
    
    if (keyNamesSize > 0) {
        wchar_t* currentKey = keyNames;
        while (*currentKey) {
            std::wstring tabName = currentKey;
            wchar_t colorValue[32] = {0};
            GetPrivateProfileString(L"TabColors", tabName.c_str(), L"", colorValue, 32, iniPathPtr);
            
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
    
    const wchar_t* iniPathPtr = iniPath.c_str();
    
    // Window settings
    WritePrivateProfileString(L"Window", L"X", std::to_wstring(windowX).c_str(), iniPathPtr);
    WritePrivateProfileString(L"Window", L"Y", std::to_wstring(windowY).c_str(), iniPathPtr);
    WritePrivateProfileString(L"Window", L"Width", std::to_wstring(windowWidth).c_str(), iniPathPtr);
    WritePrivateProfileString(L"Window", L"Height", std::to_wstring(windowHeight).c_str(), iniPathPtr);
    WritePrivateProfileString(L"Window", L"ActiveTab", std::to_wstring(activeTab).c_str(), iniPathPtr);
    
    // Color settings
    DWORD activeColorHex = (GetRValue(tabActiveColor) << 16) | (GetGValue(tabActiveColor) << 8) | GetBValue(tabActiveColor);
    DWORD inactiveColorHex = (GetRValue(tabInactiveColor) << 16) | (GetGValue(tabInactiveColor) << 8) | GetBValue(tabInactiveColor);
    
    wchar_t activeColorStr[16];
    wchar_t inactiveColorStr[16];
    swprintf_s(activeColorStr, L"0x%X", activeColorHex);
    swprintf_s(inactiveColorStr, L"0x%X", inactiveColorHex);
    
    WritePrivateProfileString(L"Colors", L"TabActiveColor", activeColorStr, iniPathPtr);
    WritePrivateProfileString(L"Colors", L"TabInactiveColor", inactiveColorStr, iniPathPtr);
    
    // Display settings
    wchar_t iconScaleStr[32];
    swprintf_s(iconScaleStr, L"%.2f", iconScale);
    WritePrivateProfileString(L"Display", L"IconScale", iconScaleStr, iniPathPtr);
    WritePrivateProfileString(L"Display", L"IconLabelFontSize", std::to_wstring(iconLabelFontSize).c_str(), iniPathPtr);
    WritePrivateProfileString(L"Display", L"TabFontSize", std::to_wstring(tabFontSize).c_str(), iniPathPtr);
    WritePrivateProfileString(L"Display", L"IconSpacingHorizontal", std::to_wstring(iconSpacingHorizontal).c_str(), iniPathPtr);
    WritePrivateProfileString(L"Display", L"IconSpacingVertical", std::to_wstring(iconSpacingVertical).c_str(), iniPathPtr);
    WritePrivateProfileString(L"Display", L"TabHeight", std::to_wstring(tabHeight).c_str(), iniPathPtr);
    WritePrivateProfileString(L"Display", L"IconVerticalPadding", std::to_wstring(iconVerticalPadding).c_str(), iniPathPtr);
    
    // Scrolling settings
    WritePrivateProfileString(L"Scrolling", L"MouseScrollSpeed", std::to_wstring(mouseScrollSpeed).c_str(), iniPathPtr);
    WritePrivateProfileString(L"Scrolling", L"JoystickScrollSpeed", std::to_wstring(joystickScrollSpeed).c_str(), iniPathPtr);
    
    // Tab-specific colors
    for (const auto& pair : tabSpecificColors) {
        DWORD tabColorHex = (GetRValue(pair.second) << 16) | (GetGValue(pair.second) << 8) | GetBValue(pair.second);
        wchar_t tabColorStr[16];
        swprintf_s(tabColorStr, L"0x%X", tabColorHex);
        WritePrivateProfileString(L"TabColors", pair.first.c_str(), tabColorStr, iniPathPtr);
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
