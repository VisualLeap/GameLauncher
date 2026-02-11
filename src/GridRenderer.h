// GridRenderer.h - Grid rendering with modern aesthetics
#pragma once

#include <windows.h>
#include <vector>
#include "DataModels.h"

class GridRenderer {
public:
    GridRenderer();
    ~GridRenderer();

    void SetShortcuts(std::vector<ShortcutInfo>* shortcuts);
    void SetScrollOffset(int offset) { scrollOffset = offset; }
    void SetSelectedIcon(int index) { selectedIconIndex = index; }
    void SetDpiScaleFactor(float scaleFactor) { dpiScaleFactor = scaleFactor; }
    void Render(HDC hdc, const RECT& clientRect);
    int GetClickedShortcut(POINT clickPoint, const RECT& clientRect);
    
    // Get the rectangle for a specific icon (including label area)
    RECT GetIconBounds(int index, const RECT& clientRect);

private:
    std::vector<ShortcutInfo>* shortcuts; // Non-owning pointer
    int selectedIconIndex;
    int scrollOffset; // Vertical scroll offset in pixels
    float dpiScaleFactor; // DPI scaling factor for this window
    
    // Layout calculation
    void CalculateGridLayout(const RECT& rect, int& cols, int& rows, int& startX, int& startY);
    RECT GetIconRect(int index, int cols, int startX, int startY);
    
    // Modern rendering effects
    void DrawIconWithModernEffects(HDC hdc, HICON icon, const RECT& iconRect, bool isHovered, bool isSelected);
    void DrawHoverEffect(HDC hdc, const RECT& iconRect);
    void DrawSelectionEffect(HDC hdc, const RECT& iconRect);
    void DrawIconLabel(HDC hdc, const std::wstring& text, const RECT& iconRect);
    
    // Helper functions
    void DrawRect(HDC hdc, const RECT& rect, COLORREF color);
    
    // Constants from design - now DPI-aware
    int GetPhysicalIconSize() const { return DesignConstants::TARGET_ICON_SIZE_PIXELS; }
    static const int ICON_PADDING = DesignConstants::ICON_PADDING;
    static const int GRID_MARGIN = DesignConstants::GRID_MARGIN;
    static const int LABEL_HEIGHT = 50;  // Increased to accommodate 2-3 lines
    int GetTotalItemHeight() const { return GetPhysicalIconSize() + LABEL_HEIGHT + 8; } // 8px spacing
};