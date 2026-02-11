// GridRenderer.cpp - Grid rendering implementation with modern aesthetics
#include "GridRenderer.h"
#include <algorithm>
#include <cmath>

GridRenderer::GridRenderer() 
    : shortcuts(nullptr)
    , selectedIconIndex(-1)
    , scrollOffset(0)
    , dpiScaleFactor(1.0f)
{
}

GridRenderer::~GridRenderer() {
}

void GridRenderer::SetShortcuts(std::vector<ShortcutInfo>* shortcutList) {
    shortcuts = shortcutList;
}

void GridRenderer::Render(HDC hdc, const RECT& clientRect) {

    // Set up text rendering (common for both branches)
    SetBkMode(hdc, TRANSPARENT);
    
    HFONT hFont = CreateFont(28, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    if (!shortcuts || shortcuts->empty()) {
        // Draw "No shortcuts found" message
        SetTextColor(hdc, RGB(128, 128, 128));
        
        std::wstring message = L"No shortcuts found in the configured folder";
        RECT textRect = clientRect;
        DrawText(hdc, message.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        // Cleanup and return
        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
        return;
    }
    
    // Set text color for icon labels
    SetTextColor(hdc, RGB(255, 255, 255));
    
    int cols, rows, startX, startY;
    CalculateGridLayout(clientRect, cols, rows, startX, startY);
    
    // Render each shortcut (only render visible ones for performance)
    for (size_t i = 0; i < shortcuts->size(); ++i) {
        const auto& shortcut = (*shortcuts)[i];
        
        RECT iconRect = GetIconRect(static_cast<int>(i), cols, startX, startY);
        
        // Skip rendering if icon is completely outside the visible grid area
        if (iconRect.bottom < clientRect.top || iconRect.top > clientRect.bottom ||
            iconRect.right < clientRect.left || iconRect.left > clientRect.right) {
            continue;
        }
        
        bool isSelected = (static_cast<int>(i) == selectedIconIndex);
        
        // Draw the icon with modern effects
        if (shortcut.largeIcon) {
            DrawIconWithModernEffects(hdc, shortcut.largeIcon, iconRect, false, isSelected);
            
            // Draw selection indicator
            if (isSelected) {
                RECT selectionRect = iconRect;
                InflateRect(&selectionRect, 3, 3);
                HPEN selectionPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
                HPEN oldPen = (HPEN)SelectObject(hdc, selectionPen);
                HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                
                Rectangle(hdc, selectionRect.left, selectionRect.top, selectionRect.right, selectionRect.bottom);
                
                SelectObject(hdc, oldPen);
                SelectObject(hdc, oldBrush);
                DeleteObject(selectionPen);
            }
        } else {
            // Draw placeholder for missing icon
            DrawRect(hdc, iconRect, RGB(64, 64, 64));
        }
        
        // Draw label below icon
        RECT labelRect = iconRect;
        labelRect.top = iconRect.bottom + DesignConstants::SELECTION_BORDER_PADDING;
        labelRect.bottom = labelRect.top + DesignConstants::LABEL_HEIGHT;
        
        // Only draw label if it's visible within the grid area
        if (labelRect.top < clientRect.bottom && labelRect.bottom > clientRect.top &&
            labelRect.right > clientRect.left && labelRect.left < clientRect.right) {
            DrawIconLabel(hdc, shortcut.displayName, labelRect);
        }
    }
    
    // Cleanup
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

int GridRenderer::GetClickedShortcut(POINT clickPoint, const RECT& clientRect) {
    if (!shortcuts || shortcuts->empty()) {
        return -1;
    }
    
    int cols, rows, startX, startY;
    CalculateGridLayout(clientRect, cols, rows, startX, startY);
    
    for (size_t i = 0; i < shortcuts->size(); ++i) {
        RECT iconRect = GetIconRect(static_cast<int>(i), cols, startX, startY);
        
        // Expand click area to include label
        iconRect.bottom += DesignConstants::LABEL_HEIGHT + DesignConstants::SELECTION_BORDER_PADDING;
        
        if (PtInRect(&iconRect, clickPoint)) {
            return static_cast<int>(i);
        }
    }
    
    return -1;
}

RECT GridRenderer::GetIconBounds(int index, const RECT& clientRect) {
    if (!shortcuts || shortcuts->empty() || index < 0 || index >= static_cast<int>(shortcuts->size())) {
        return {0, 0, 0, 0}; // Empty rectangle
    }
    
    int cols, rows, startX, startY;
    CalculateGridLayout(clientRect, cols, rows, startX, startY);
    
    RECT iconRect = GetIconRect(index, cols, startX, startY);
    
    // Expand to include label area and some padding for hover effects
    iconRect.bottom += DesignConstants::LABEL_HEIGHT + DesignConstants::LABEL_SPACING + DesignConstants::SELECTION_BORDER_PADDING;
    iconRect.left -= DesignConstants::SELECTION_BORDER_PADDING;
    iconRect.right += DesignConstants::SELECTION_BORDER_PADDING;
    iconRect.top -= DesignConstants::SELECTION_BORDER_PADDING;
    
    return iconRect;
}

void GridRenderer::CalculateGridLayout(const RECT& rect, int& cols, int& rows, int& startX, int& startY) {
    if (!shortcuts || shortcuts->empty()) {
        cols = rows = startX = startY = 0;
        return;
    }
    
    // Use the full rect since WindowManager now handles margins
    int availableWidth = rect.right - rect.left;
    
    // Calculate columns based on available width using DPI-aware icon size
    int physicalIconSize = GetPhysicalIconSize();
    int itemWidth = physicalIconSize + ICON_PADDING;
    cols = (availableWidth / itemWidth > 1) ? (availableWidth / itemWidth) : 1;
    
    // Calculate rows needed
    int shortcutCount = static_cast<int>(shortcuts->size());
    rows = (shortcutCount + cols - 1) / cols; // Ceiling division
    
    // Center the grid horizontally within the provided rect
    int totalGridWidth = cols * itemWidth - ICON_PADDING;
    startX = rect.left + (availableWidth - totalGridWidth) / 2;
    
    // Start from top of the rect with small padding to prevent selection border clipping
    startY = rect.top + DesignConstants::SELECTION_BORDER_PADDING;
}

RECT GridRenderer::GetIconRect(int index, int cols, int startX, int startY) {
    int row = index / cols;
    int col = index % cols;
    
    int physicalIconSize = GetPhysicalIconSize();
    int itemWidth = physicalIconSize + ICON_PADDING;
    int itemHeight = GetTotalItemHeight() + ICON_PADDING;
    
    RECT iconRect;
    iconRect.left = startX + col * itemWidth;
    iconRect.top = startY + row * itemHeight - scrollOffset; // Apply scroll offset
    iconRect.right = iconRect.left + physicalIconSize;
    iconRect.bottom = iconRect.top + physicalIconSize;
    
    return iconRect;
}

void GridRenderer::DrawIconWithModernEffects(HDC hdc, HICON icon, const RECT& iconRect, bool isHovered, bool isSelected) {
    // Draw selection border if selected
    if (isSelected) {
        RECT selectionRect = iconRect;
        InflateRect(&selectionRect, 2, 2);
        HBRUSH selectionBrush = CreateSolidBrush(DesignConstants::ACCENT_COLOR);
        FrameRect(hdc, &selectionRect, selectionBrush);
        DeleteObject(selectionBrush);
    }
    
    // Draw hover border if hovered
    if (isHovered) {
        RECT hoverRect = iconRect;
        InflateRect(&hoverRect, 1, 1);
        HBRUSH hoverBrush = CreateSolidBrush(DesignConstants::HOVER_COLOR);
        FrameRect(hdc, &hoverRect, hoverBrush);
        DeleteObject(hoverBrush);
    }
    
    // Draw the icon at DPI-aware size - this ensures it's always 256 physical pixels
    int physicalIconSize = GetPhysicalIconSize();
    
    DrawIconEx(hdc, iconRect.left, iconRect.top, icon, physicalIconSize, physicalIconSize, 0, nullptr, DI_NORMAL);
}

void GridRenderer::DrawHoverEffect(HDC hdc, const RECT& iconRect) {
    RECT hoverRect = iconRect;
    InflateRect(&hoverRect, 4, 4);
    
    // Draw glowing border effect
    DrawRect(hdc, hoverRect, DesignConstants::HOVER_COLOR);
}

void GridRenderer::DrawSelectionEffect(HDC hdc, const RECT& iconRect) {
    RECT selectionRect = iconRect;
    InflateRect(&selectionRect, 6, 6);
    
    // Draw accent color border for selection
    DrawRect(hdc, selectionRect, DesignConstants::ACCENT_COLOR);
}

void GridRenderer::DrawIconLabel(HDC hdc, const std::wstring& text, const RECT& labelRect) {
    if (text.empty()) {
        return;
    }
    
    // Use the full text without truncation
    std::wstring displayText = text;
    
    // Draw text centered with word wrapping
    RECT textRect = labelRect;
    DrawText(hdc, displayText.c_str(), -1, &textRect, 
             DT_CENTER | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
}

void GridRenderer::DrawRect(HDC hdc, const RECT& rect, COLORREF color) {
    
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    
    // Simple rectangle for now - can be enhanced with rounded corners later
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    
    SelectObject(hdc, oldBrush);
    DeleteObject(brush);
}