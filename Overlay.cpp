#include "Includes.h"
#include "Overlay.h"
#include <algorithm>

std::wstring g_overlay_text;

#ifdef IMAGINA_LINUX
// Linux Phase 3.5: FreeType-rendered in-image overlay.
// Mirrors Windows path: dark backdrop + Consolas-bold-ish text top-left.
#include <ft2build.h>
#include FT_FREETYPE_H
#include <mutex>
#include <vector>

namespace {
	std::once_flag g_ft_init_flag;
	FT_Library     g_ft_lib  = nullptr;
	FT_Face        g_ft_face = nullptr;
	int            g_ft_face_size = -1;

	void init_freetype() {
		if (FT_Init_FreeType(&g_ft_lib)) { g_ft_lib = nullptr; return; }
		const char *paths[] = {
			"/usr/share/fonts/TTF/DejaVuSansMono-Bold.ttf",
			"/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",
			"/usr/share/fonts/dejavu/DejaVuSansMono-Bold.ttf",
		};
		for (auto p : paths) {
			if (FT_New_Face(g_ft_lib, p, 0, &g_ft_face) == 0) return;
			g_ft_face = nullptr;
		}
	}
}

void DrawTextOverlay(uint8_t *rgba, int width, int height,
                     const wchar_t *text, int x, int y) {
	if (!text || !*text || !rgba || width <= 0 || height <= 0) return;

	std::call_once(g_ft_init_flag, init_freetype);
	if (!g_ft_face) return;

	int pixel_size = std::clamp(height / 35, 22, 64);
	if (pixel_size != g_ft_face_size) {
		FT_Set_Pixel_Sizes(g_ft_face, 0, pixel_size);
		g_ft_face_size = pixel_size;
	}

	// SaveImage writes bottom-up (row_pointers flipped). Our (x, y) is in
	// PNG top-left coordinates; flip Y when indexing rgba.
	auto flipY = [&](int py) { return (height - 1) - py; };

	// Measure first (to size the backdrop box).
	int adv_x = x;
	int line_top    = y;
	int line_bottom = y + pixel_size;
	int baseline    = y + (g_ft_face->size->metrics.ascender >> 6);
	for (const wchar_t *p = text; *p; p++) {
		if (FT_Load_Char(g_ft_face, (FT_ULong)*p, FT_LOAD_DEFAULT)) continue;
		adv_x += g_ft_face->glyph->advance.x >> 6;
	}
	int text_width = adv_x - x;

	// Dark backdrop: scale R/G/B down to ~39% under the text region.
	const int pad = 8;
	int box_x0 = std::max(0, x - pad);
	int box_y0 = std::max(0, line_top - pad);
	int box_x1 = std::min(width,  x + text_width + pad);
	int box_y1 = std::min(height, line_bottom + pad);
	for (int py = box_y0; py < box_y1; py++) {
		uint8_t *row = rgba + (size_t)flipY(py) * width * 4;
		for (int px = box_x0; px < box_x1; px++) {
			row[px*4 + 0] = (uint8_t)(row[px*4 + 0] * 100 / 255);
			row[px*4 + 1] = (uint8_t)(row[px*4 + 1] * 100 / 255);
			row[px*4 + 2] = (uint8_t)(row[px*4 + 2] * 100 / 255);
		}
	}

	// Render glyphs.
	int pen_x = x;
	for (const wchar_t *p = text; *p; p++) {
		if (FT_Load_Char(g_ft_face, (FT_ULong)*p, FT_LOAD_RENDER)) continue;
		FT_GlyphSlot g = g_ft_face->glyph;
		int gx = pen_x + g->bitmap_left;
		int gy = baseline - g->bitmap_top;
		for (unsigned int row = 0; row < g->bitmap.rows; row++) {
			int dy = gy + (int)row;
			if (dy < 0 || dy >= height) continue;
			uint8_t *dst_row = rgba + (size_t)flipY(dy) * width * 4;
			for (unsigned int col = 0; col < g->bitmap.width; col++) {
				int dx = gx + (int)col;
				if (dx < 0 || dx >= width) continue;
				uint8_t a = g->bitmap.buffer[row * g->bitmap.pitch + col];
				if (a == 0) continue;
				uint8_t *d = &dst_row[dx * 4];
				int inv = 255 - a;
				d[0] = (uint8_t)((d[0] * inv + 255 * a) / 255);
				d[1] = (uint8_t)((d[1] * inv + 255 * a) / 255);
				d[2] = (uint8_t)((d[2] * inv + 255 * a) / 255);
				d[3] = 255;
			}
		}
		pen_x += g->advance.x >> 6;
	}
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
