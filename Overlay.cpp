#include "Includes.h"
#include "Overlay.h"
#include <algorithm>

std::wstring g_overlay_text;

#ifdef IMAGINA_LINUX
// Linux Phase 2: skip in-image text overlay. Re-introduce in Phase 3 with FreeType.
void DrawTextOverlay(uint8_t * /*rgba*/, int /*width*/, int /*height*/,
                     const wchar_t * /*text*/, int /*x*/, int /*y*/) {
}
#else
void DrawTextOverlay(uint8_t *rgba, int width, int height,
                     const wchar_t *text, int x, int y) {
	if (!text || !*text || !rgba || width <= 0 || height <= 0) return;

	// Top-down DIB.
	HDC screenDC = GetDC(nullptr);
	HDC memDC = CreateCompatibleDC(screenDC);
	ReleaseDC(nullptr, screenDC);

	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth       = width;
	bmi.bmiHeader.biHeight      = -height; // top-down.
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biBitCount    = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void *dibPixels = nullptr;
	HBITMAP dib = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &dibPixels, nullptr, 0);
	if (!dib) { DeleteDC(memDC); return; }
	HGDIOBJ oldBmp = SelectObject(memDC, dib);

	memset(dibPixels, 0, (size_t)width * height * 4);

	// Consolas bold font.
	int fontHeight = std::clamp(height / 35, 22, 64);
	HFONT font = CreateFontW(
		-fontHeight, 0, 0, 0, FW_BOLD,
		FALSE, FALSE, FALSE,
		DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY, DEFAULT_PITCH,
		L"Consolas");
	HGDIOBJ oldFont = SelectObject(memDC, font);

	SetBkMode(memDC, TRANSPARENT);
	SetTextColor(memDC, RGB(255, 255, 255));

	int textLen = (int)wcslen(text);

	// Measure text.
	SIZE textSize{};
	GetTextExtentPoint32W(memDC, text, textLen, &textSize);

	// Render onto DIB.
	RECT rc{ x, y, x + textSize.cx, y + textSize.cy };
	DrawTextW(memDC, text, textLen, &rc, DT_LEFT | DT_TOP | DT_NOCLIP);

	// Composite onto RGBA.
	const int pad = 8;
	int box_x = std::max(0, x - pad);
	int box_y = std::max(0, y - pad);
	int box_x1 = std::min<int>(width,  int(x + textSize.cx + pad));
	int box_y1 = std::min<int>(height, int(y + textSize.cy + pad));

	// Flip Y for PNG bottom-up.
	auto flipY = [&](int py) { return (height - 1) - py; };

	// Dark backdrop.
	for (int py = box_y; py < box_y1; py++) {
		uint8_t *row = rgba + (size_t)flipY(py) * width * 4;
		for (int px = box_x; px < box_x1; px++) {
			row[px * 4 + 0] = (uint8_t)(row[px * 4 + 0] * 100 / 255);
			row[px * 4 + 1] = (uint8_t)(row[px * 4 + 1] * 100 / 255);
			row[px * 4 + 2] = (uint8_t)(row[px * 4 + 2] * 100 / 255);
		}
	}

	// Stamp glyphs.
	const uint8_t *src = (const uint8_t *)dibPixels;
	for (int py = 0; py < height; py++) {
		const uint8_t *srcRow = src + (size_t)py * width * 4;
		uint8_t *dstRow = rgba + (size_t)flipY(py) * width * 4;
		for (int px = 0; px < width; px++) {
			uint8_t b = srcRow[px * 4 + 0];
			uint8_t g = srcRow[px * 4 + 1];
			uint8_t r = srcRow[px * 4 + 2];
			int brightness = (r + g + b);
			if (brightness == 0) continue;
			// Brightness as alpha.
			int a = std::min(255, brightness / 2);
			int invA = 255 - a;
			uint8_t *dst = &dstRow[px * 4];
			dst[0] = (uint8_t)((dst[0] * invA + 255 * a) / 255);
			dst[1] = (uint8_t)((dst[1] * invA + 255 * a) / 255);
			dst[2] = (uint8_t)((dst[2] * invA + 255 * a) / 255);
			dst[3] = 255;
		}
	}

	// Cleanup.
	SelectObject(memDC, oldFont);
	DeleteObject(font);
	SelectObject(memDC, oldBmp);
	DeleteObject(dib);
	DeleteDC(memDC);
}
#endif
