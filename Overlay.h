#pragma once
#include <string>
#include <cstdint>

// Burned-in PNG overlay text.
extern std::wstring g_overlay_text;

// Stamp text onto RGBA buffer.
void DrawTextOverlay(uint8_t *rgba, int width, int height,
                     const wchar_t *text, int x, int y);
