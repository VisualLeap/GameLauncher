// ShortcutParser.h - Windows shortcut (.lnk) file parser
#pragma once

#include <windows.h>
#include <shlobj.h>
#include <string>
#include "DataModels.h"

class ShortcutParser {
public:
    ShortcutParser();
    ~ShortcutParser();

    bool Initialize();
    void Cleanup();
    bool ParseShortcut(const std::wstring& shortcutPath, ShortcutInfo& info);

private:
    bool comInitialized;
    IShellLink* shellLink;
    IPersistFile* persistFile;
    
    bool InitializeCOM();
    void CleanupCOM();
    bool CreateShellLinkInterface();
    void ReleaseShellLinkInterface();
    
    std::wstring GetFileNameFromPath(const std::wstring& path);
    bool FileExists(const std::wstring& path);
};