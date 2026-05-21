#pragma once
#include <string>
#include <cstdint>

// CLI arguments.

struct CLIArgs {
	bool valid = true;
	std::wstring error;

	// Action flags.
	bool auto_render = false;
	bool render_zoom = false;
	bool render_done = false;

	// Input.
	std::wstring location_file;

	// Render config.
	int      width = 0;
	int      height = 0;
	uint64_t iterations = 0;

	// Optional start center.
	bool     has_center = false;
	double   center_x = 0.0;
	double   center_y = 0.0;

	// Start zoom.
	double   zoom_start = 0.0;

	// Single still output.
	std::wstring output_file;

	// Frame sequence output.
	std::wstring frames_dir;
	uint64_t frames_total = 0;
	uint64_t frame_index = 0;
	double   zoom_step = 0.97;
};

extern CLIArgs g_cli;

// Parse command line.
void ParseCLI(int argc, wchar_t **argv);

// Apply parsed settings.
void ApplyCLISettings();

// Save and quit if done.
bool CheckAutoRenderComplete();

// Advance frame sequence.
bool CheckFrameSequenceProgress();

// Update window title.
void UpdateWindowTitle();

// Show help dialog.
void ShowHelp();
