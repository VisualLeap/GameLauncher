// ShortcutScanner.h - Shortcut scanning and parsing
#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include "DataModels.h"

class IconExtractor;
class ShortcutParser;
class WindowManager;

class ShortcutScanner {
public:
    ShortcutScanner();
    ~ShortcutScanner();

    bool Initialize();
    bool SetFolder(const std::wstring& folderPath);
    void SetWindowManager(WindowManager* windowMgr) { windowManager = windowMgr; }
    std::vector<ShortcutInfo> ScanShortcuts();
    std::vector<TabInfo> ScanTabs();  // New method for tab scanning
    
    const std::wstring& GetFolder() const { return scanFolder; }
    size_t GetLastScanCount() const { return lastScanCount; }

private:
    std::wstring scanFolder;
    std::unique_ptr<IconExtractor> iconExtractor;
    std::unique_ptr<ShortcutParser> parser;
    WindowManager* windowManager;
    size_t lastScanCount;
    
    bool IsShortcutFile(const std::wstring& filename);
    std::vector<std::wstring> FindShortcutFiles();
    std::vector<std::wstring> FindSubfolders();  // New method
    std::vector<ShortcutInfo> ScanFolderForShortcuts(const std::wstring& folderPath);  // New method
    bool ProcessShortcutFile(const std::wstring& filePath, ShortcutInfo& info);
};