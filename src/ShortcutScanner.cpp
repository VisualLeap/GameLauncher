// ShortcutScanner.cpp - Shortcut scanning implementation
#include "ShortcutScanner.h"
#include "ShortcutParser.h"
#include "IconExtractor.h"
#include <filesystem>
#include <algorithm>

ShortcutScanner::ShortcutScanner() 
    : lastScanCount(0)
{
}

ShortcutScanner::~ShortcutScanner() {
}

bool ShortcutScanner::Initialize() {
    // Create parser and icon extractor
    parser = std::make_unique<ShortcutParser>();
    iconExtractor = std::make_unique<IconExtractor>();
    
    // Initialize parser
    if (!parser->Initialize()) {
        return false;
    }
    
    // Clear icon cache to ensure fresh extraction
    iconExtractor->ClearCache();
    
    return true;
}

bool ShortcutScanner::SetFolder(const std::wstring& folderPath) {
    scanFolder = folderPath;
    
    // Check if folder exists
    DWORD attributes = GetFileAttributes(folderPath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return false;
    }
    
    return true;
}

std::vector<ShortcutInfo> ShortcutScanner::ScanShortcuts() {
    std::vector<ShortcutInfo> shortcuts;
    lastScanCount = 0;
    
    if (scanFolder.empty()) {
        return shortcuts;
    }
    
    // Find all .lnk files in the folder
    std::vector<std::wstring> shortcutFiles = FindShortcutFiles();
    
    // Process each shortcut file
    for (const auto& filePath : shortcutFiles) {
        ShortcutInfo info;
        
        if (ProcessShortcutFile(filePath, info)) {
            shortcuts.emplace_back(std::move(info));
            lastScanCount++;
        }
    }
    
    return shortcuts;
}

std::vector<TabInfo> ShortcutScanner::ScanTabs() {
    std::vector<TabInfo> tabs;
    
    // Clear icon cache to force fresh extraction for debugging
    if (iconExtractor) {
        iconExtractor->ClearCache();
    }
    
    if (scanFolder.empty()) {
        return tabs;
    }
    
    // Check if the scan folder exists
    DWORD attributes = GetFileAttributes(scanFolder.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        MessageBox(nullptr, (L"Scan folder does not exist: " + scanFolder).c_str(), L"Debug", MB_OK);
        return tabs;
    }
    
    // First, add a tab for root folder shortcuts (if any exist)
    
    std::vector<ShortcutInfo> rootShortcuts = ScanFolderForShortcuts(scanFolder);
    
    if (!rootShortcuts.empty()) {
        TabInfo rootTab;
        rootTab.name = L"All";  // Generic name for root folder
        rootTab.folderPath = scanFolder;
        rootTab.shortcuts = std::move(rootShortcuts);
        tabs.emplace_back(std::move(rootTab));
    }
    
    // Find all subfolders
    std::vector<std::wstring> subfolders = FindSubfolders();
    
    // Create a tab for each subfolder that contains shortcuts
    for (const auto& folderPath : subfolders) {
        std::vector<ShortcutInfo> folderShortcuts = ScanFolderForShortcuts(folderPath);
        
        if (!folderShortcuts.empty()) {
            TabInfo tab;
            
            // Extract folder name from path
            std::filesystem::path path(folderPath);
            tab.name = path.filename().wstring();
            tab.folderPath = folderPath;
            tab.shortcuts = std::move(folderShortcuts);
            
            tabs.emplace_back(std::move(tab));
        }
    }
    
    return tabs;
}

std::vector<std::wstring> ShortcutScanner::FindSubfolders() {
    std::vector<std::wstring> subfolders;
    
    try {
        std::filesystem::path scanPath(scanFolder);
        
        if (!std::filesystem::exists(scanPath) || !std::filesystem::is_directory(scanPath)) {
            return subfolders;
        }
        
        // Iterate through entries in the directory
        for (const auto& entry : std::filesystem::directory_iterator(scanPath)) {
            if (entry.is_directory()) {
                subfolders.push_back(entry.path().wstring());
            }
        }
        
        // Sort folders alphabetically for consistent ordering
        std::sort(subfolders.begin(), subfolders.end());
        
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore filesystem errors
    } catch (const std::exception&) {
        // Ignore errors
    }
    
    return subfolders;
}

std::vector<ShortcutInfo> ShortcutScanner::ScanFolderForShortcuts(const std::wstring& folderPath) {
    std::vector<ShortcutInfo> shortcuts;
    
    try {
        std::filesystem::path scanPath(folderPath);
        
        if (!std::filesystem::exists(scanPath)) {
            return shortcuts;
        }
        
        if (!std::filesystem::is_directory(scanPath)) {
            return shortcuts;
        }
        
        // Find all .lnk files in this specific folder
        std::vector<std::wstring> shortcutFiles;
        
        for (const auto& entry : std::filesystem::directory_iterator(scanPath)) {
            if (entry.is_regular_file()) {
                std::wstring filename = entry.path().filename().wstring();
                std::wstring fullPath = entry.path().wstring();
                
                if (IsShortcutFile(filename)) {
                    shortcutFiles.push_back(fullPath);
                }
            }
        }
        
        // Sort files alphabetically
        std::sort(shortcutFiles.begin(), shortcutFiles.end());
        
        // Process each shortcut file
        for (const auto& filePath : shortcutFiles) {
            ShortcutInfo info;
            
            if (ProcessShortcutFile(filePath, info)) {
                shortcuts.emplace_back(std::move(info));
            }
        }
        
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore filesystem errors
    } catch (const std::exception&) {
        // Ignore errors
    }
    
    return shortcuts;
}

bool ShortcutScanner::IsShortcutFile(const std::wstring& filename) {
    if (filename.length() < 4) {
        return false;
    }
    
    std::wstring extension = filename.substr(filename.length() - 4);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    
    return extension == L".lnk";
}

std::vector<std::wstring> ShortcutScanner::FindShortcutFiles() {
    std::vector<std::wstring> shortcutFiles;
    
    try {
        std::filesystem::path scanPath(scanFolder);
        
        if (!std::filesystem::exists(scanPath) || !std::filesystem::is_directory(scanPath)) {
            return shortcutFiles;
        }
        
        // Iterate through files in the directory
        for (const auto& entry : std::filesystem::directory_iterator(scanPath)) {
            if (entry.is_regular_file()) {
                std::wstring filename = entry.path().filename().wstring();
                
                if (IsShortcutFile(filename)) {
                    shortcutFiles.push_back(entry.path().wstring());
                }
            }
        }
        
        // Sort files alphabetically for consistent ordering
        std::sort(shortcutFiles.begin(), shortcutFiles.end());
        
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore filesystem errors
    } catch (const std::exception&) {
        // Ignore errors
    }
    
    return shortcutFiles;
}

bool ShortcutScanner::ProcessShortcutFile(const std::wstring& filePath, ShortcutInfo& info) {
    if (!parser) {
        return false;
    }
    
    // Parse the shortcut to get basic information
    if (!parser->ParseShortcut(filePath, info)) {
        return false;
    }
    
    // Extract icon if we have an icon extractor
    if (iconExtractor) {
        HICON icon = nullptr;
        
        // Simplified logic: If shortcut has custom icon, use it; otherwise use exe icon
        if (!info.iconPath.empty()) {
            // Custom icon specified - load from .ico file
            icon = iconExtractor->ExtractFromIconFile(info.iconPath);
        } else if (!info.targetPath.empty()) {
            // No custom icon - extract from target executable
            icon = iconExtractor->ExtractFromExecutable(info.targetPath, info.iconIndex);
        }
        
        info.largeIcon = icon;
    }
    
    return true;
}