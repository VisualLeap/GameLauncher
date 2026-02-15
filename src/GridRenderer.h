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
    
    // Cached GDI objects for performance
    HFONT cachedFont;
    HPEN cachedSelectionPen;
    HPEN cachedShadowPen;
    
    // Layout calculation
    void CalculateGridLayout(const RECT& rect, int& cols, int& rows, int& startX, int& startY);
    RECT GetIconRect(int index, int cols, int startX, int startY);
    
    // Modern rendering effects
    void DrawIconWithModernEffects(HDC hdc, HBITMAP iconBitmap, int bitmapWidth, int bitmapHeight, 
                                   const RECT& iconRect, bool isHovered, bool isSelected);
    void DrawIconLabel(HDC hdc, const std::wstring& text, const RECT& iconRect);
    
    // Helper functions
    void DrawRect(HDC hdc, const RECT& rect, COLORREF color);
    
    // Constants from design - now DPI-aware
    int GetPhysicalIconSize() const { return DesignConstants::TARGET_ICON_SIZE_PIXELS; }
    static const int ICON_PADDING = DesignConstants::ICON_PADDING;
    static const int GRID_MARGIN = DesignConstants::GRID_MARGIN;
    int GetTotalItemHeight() const { return GetPhysicalIconSize() + DesignConstants::LABEL_HEIGHT + DesignConstants::LABEL_SPACING; }
};