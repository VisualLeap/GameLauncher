// IconExtractor.h - Simplified icon extraction from shortcuts and executables
#pragma once

#include <windows.h>
#include <string>
#include <unordered_map>
#include <shellapi.h>
#include <shlobj.h>
#include "DataModels.h"

class WindowManager;

class IconExtractor {
public:
    IconExtractor();
    ~IconExtractor();

    void SetWindowManager(WindowManager* windowMgr) { windowManager = windowMgr; }
    HICON ExtractFromExecutable(const std::wstring& exePath, int iconIndex = 0);
    HICON ExtractFromIconFile(const std::wstring& iconPath);
    
    void ClearCache();
    size_t GetCacheSize() const { return iconCache.size(); }

private:
    // Icon cache to avoid repeated extractions
    std::unordered_map<std::wstring, HICON> iconCache;
    WindowManager* windowManager;
    
    HICON ExtractIconFromPE(const std::wstring& filePath);
    HICON LoadIconFromFile(const std::wstring& iconPath);
    bool ValidateIconSize(HICON icon, const std::wstring& filePath);
    void ShowIconSizeError(const std::wstring& filePath, int actualWidth, int actualHeight);
    
    std::wstring GenerateCacheKey(const std::wstring& filePath, int iconIndex);
    bool IsValidIcon(HICON icon);
    
    // Constants
    static const int REQUIRED_ICON_SIZE = DesignConstants::TARGET_ICON_SIZE_PIXELS;
};