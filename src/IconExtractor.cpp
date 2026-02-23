// IconExtractor.cpp - Simplified icon extraction implementation
#include "IconExtractor.h"
#include <shellapi.h>

IconExtractor::IconExtractor() {
}

IconExtractor::~IconExtractor() {
    ClearCache();
}

HICON IconExtractor::ExtractFromExecutable(const std::wstring& exePath, int iconIndex) {
    if (exePath.empty()) {
        return nullptr;
    }
    
    // Check cache first
    std::wstring cacheKey = GenerateCacheKey(exePath, iconIndex);
    auto cacheIt = iconCache.find(cacheKey);
    if (cacheIt != iconCache.end()) {
        return cacheIt->second;
    }
    
    // Extract icon from PE resources
    HICON icon = ExtractIconFromPE(exePath);
    if (!icon) {
        return nullptr;
    }
    
    // Cache and return
    iconCache[cacheKey] = icon;
    return icon;
}

HICON IconExtractor::ExtractFromIconFile(const std::wstring& iconPath) {
    if (iconPath.empty()) {
        return nullptr;
    }
    
    // Check cache first
    std::wstring cacheKey = GenerateCacheKey(iconPath, 0);
    auto cacheIt = iconCache.find(cacheKey);
    if (cacheIt != iconCache.end()) {
        return cacheIt->second;
    }
    
    // Load icon from file
    HICON icon = LoadIconFromFile(iconPath);
    if (!icon) {
        return nullptr;
    }
    
    // Cache and return
    iconCache[cacheKey] = icon;
    return icon;
}

void IconExtractor::ClearCache() {
    for (auto& pair : iconCache) {
        if (pair.second) {
            DestroyIcon(pair.second);
        }
    }
    iconCache.clear();
}

HICON IconExtractor::ExtractIconFromPE(const std::wstring& filePath) {
    // Try to load the executable as a module to access its resources
    HMODULE hModule = LoadLibraryEx(filePath.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (!hModule) {
        return nullptr;
    }
    
    // Enumerate icon resources properly using EnumResourceNames
    struct EnumIconContext {
        HMODULE hModule;
        HICON icon256;
        std::wofstream* debugFile;
    };
    
    EnumIconContext context = { hModule, nullptr, nullptr };
    
    // Callback function for EnumResourceNames
    auto enumIconProc = [](HMODULE hModule, LPCTSTR /*lpType*/, LPTSTR lpName, LONG_PTR lParam) -> BOOL {
        EnumIconContext* ctx = reinterpret_cast<EnumIconContext*>(lParam);
        
        // lpName can be either a string or an integer ID (MAKEINTRESOURCE)
        int iconId = IS_INTRESOURCE(lpName) ? (int)(UINT_PTR)lpName : 0;
        
        if (iconId > 0) {
            // Try to load 256x256 icon
            HICON icon = (HICON)LoadImage(hModule, MAKEINTRESOURCE(iconId), IMAGE_ICON, 256, 256, LR_DEFAULTCOLOR);
            if (icon) {
                // Verify actual size
                ICONINFO iconInfo;
                if (GetIconInfo(icon, &iconInfo)) {
                    BITMAP bmp;
                    if (GetObject(iconInfo.hbmColor, sizeof(bmp), &bmp)) {
                        int actualSize = min(bmp.bmWidth, bmp.bmHeight);
                        
                        if (actualSize == 256) {
                            ctx->icon256 = icon;
                            DeleteObject(iconInfo.hbmColor);
                            DeleteObject(iconInfo.hbmMask);
                            return FALSE; // Stop enumeration - found what we need
                        } else {
                            DestroyIcon(icon);
                        }
                    }
                    DeleteObject(iconInfo.hbmColor);
                    DeleteObject(iconInfo.hbmMask);
                } else {
                    DestroyIcon(icon);
                }
            }
        }
        
        return TRUE; // Continue enumeration
    };
    
    // Enumerate all icon group resources
    EnumResourceNames(hModule, RT_GROUP_ICON, enumIconProc, (LONG_PTR)&context);
    
    HICON icon256 = context.icon256;
    
    FreeLibrary(hModule);
    return icon256;
}

HICON IconExtractor::LoadIconFromFile(const std::wstring& iconPath) {
    // Try to load icon directly using LoadImage with specific size
    HICON icon = (HICON)LoadImage(
        nullptr,
        iconPath.c_str(),
        IMAGE_ICON,
        256,
        256,
        LR_LOADFROMFILE | LR_DEFAULTCOLOR
    );
    
    return icon;
}

std::wstring IconExtractor::GenerateCacheKey(const std::wstring& filePath, int iconIndex) {
    return filePath + L":" + std::to_wstring(iconIndex);
}

bool IconExtractor::IsValidIcon(HICON icon) {
    if (!icon) {
        return false;
    }
    
    // Try to get icon info to verify it's valid
    ICONINFO iconInfo;
    bool valid = GetIconInfo(icon, &iconInfo);
    
    if (valid) {
        // Clean up the bitmaps
        if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
        if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
    }
    
    return valid;
}