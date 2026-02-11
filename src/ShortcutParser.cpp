// ShortcutParser.cpp - Windows shortcut (.lnk) file parser implementation
#include "ShortcutParser.h"
#include <comdef.h>
#include <filesystem>

ShortcutParser::ShortcutParser() 
    : comInitialized(false)
    , shellLink(nullptr)
    , persistFile(nullptr)
{
}

ShortcutParser::~ShortcutParser() {
    Cleanup();
}

bool ShortcutParser::Initialize() {
    if (!InitializeCOM()) {
        return false;
    }
    
    if (!CreateShellLinkInterface()) {
        CleanupCOM();
        return false;
    }
    
    return true;
}

void ShortcutParser::Cleanup() {
    ReleaseShellLinkInterface();
    CleanupCOM();
}

bool ShortcutParser::ParseShortcut(const std::wstring& shortcutPath, ShortcutInfo& info) {
    if (!shellLink || !persistFile) {
        return false;
    }
    
    // Check if shortcut file exists
    if (!FileExists(shortcutPath)) {
        return false;
    }
    
    // Load the shortcut file
    HRESULT hr = persistFile->Load(shortcutPath.c_str(), STGM_READ);
    if (FAILED(hr)) {
        return false;
    }
    
    // Resolve the shortcut to get the target
    hr = shellLink->Resolve(nullptr, SLR_NO_UI | SLR_NOSEARCH);
    if (FAILED(hr)) {
        // Continue anyway - we might still get some information
    }
    
    // Get target path
    wchar_t targetPath[MAX_PATH] = {0};
    hr = shellLink->GetPath(targetPath, MAX_PATH, nullptr, SLGP_UNCPRIORITY);
    if (SUCCEEDED(hr) && wcslen(targetPath) > 0) {
        info.targetPath = targetPath;
    }
    
    // Get arguments
    wchar_t arguments[MAX_PATH] = {0};
    hr = shellLink->GetArguments(arguments, MAX_PATH);
    if (SUCCEEDED(hr)) {
        info.arguments = arguments;
    }
    
    // Get working directory
    wchar_t workingDir[MAX_PATH] = {0};
    hr = shellLink->GetWorkingDirectory(workingDir, MAX_PATH);
    if (SUCCEEDED(hr)) {
        info.workingDirectory = workingDir;
    }
    
    // Get icon location
    wchar_t iconPath[MAX_PATH] = {0};
    int iconIndex = 0;
    hr = shellLink->GetIconLocation(iconPath, MAX_PATH, &iconIndex);
    if (SUCCEEDED(hr) && wcslen(iconPath) > 0) {
        info.iconPath = iconPath;
        info.iconIndex = iconIndex;
    } else {
        // If no specific icon, leave iconPath empty so we extract from target executable
        info.iconPath.clear();
        info.iconIndex = 0;
    }
    
    // Generate display name from shortcut filename
    info.displayName = GetFileNameFromPath(shortcutPath);
    
    // Remove .lnk extension from display name
    size_t lnkPos = info.displayName.rfind(L".lnk");
    if (lnkPos != std::wstring::npos) {
        info.displayName = info.displayName.substr(0, lnkPos);
    }
    
    // Check if target exists to determine validity
    info.isValid = !info.targetPath.empty() && FileExists(info.targetPath);
    
    return true;
}

bool ShortcutParser::InitializeCOM() {
    if (comInitialized) {
        return true;
    }
    
    HRESULT hr = CoInitialize(nullptr);
    if (SUCCEEDED(hr)) {
        comInitialized = true;
        return true;
    }
    
    // COM might already be initialized by the main application
    if (hr == RPC_E_CHANGED_MODE) {
        comInitialized = false; // Don't call CoUninitialize in this case
        return true;
    }
    
    return false;
}

void ShortcutParser::CleanupCOM() {
    if (comInitialized) {
        CoUninitialize();
        comInitialized = false;
    }
}

bool ShortcutParser::CreateShellLinkInterface() {
    // Create IShellLink interface
    HRESULT hr = CoCreateInstance(
        CLSID_ShellLink,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IShellLink,
        reinterpret_cast<void**>(&shellLink)
    );
    
    if (FAILED(hr)) {
        return false;
    }
    
    // Get IPersistFile interface
    hr = shellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persistFile));
    if (FAILED(hr)) {
        shellLink->Release();
        shellLink = nullptr;
        return false;
    }
    
    return true;
}

void ShortcutParser::ReleaseShellLinkInterface() {
    if (persistFile) {
        persistFile->Release();
        persistFile = nullptr;
    }
    
    if (shellLink) {
        shellLink->Release();
        shellLink = nullptr;
    }
}

std::wstring ShortcutParser::GetFileNameFromPath(const std::wstring& path) {
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        return path.substr(lastSlash + 1);
    }
    return path;
}

bool ShortcutParser::FileExists(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    
    DWORD attributes = GetFileAttributes(path.c_str());
    return (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY));
}