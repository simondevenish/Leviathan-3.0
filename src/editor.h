namespace Leviathan
{
	// forward declaration
	class Song;
}

#ifdef EDITOR_CONTROLS
// Ensure Windows headers are properly included before ImGui
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_opengl3.h"
// External function that must be defined by the Win32 backend
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace Leviathan
{
	// simpler wrapper class for the editor functionality
	class Editor
	{
	public:
		Editor();
		~Editor();

		void beginFrame(const unsigned long time);

		void endFrame(const unsigned long time);

		void printFrameStatistics();

		double handleEvents(Song* track, double position);

		void updateShaders(int* mainShaderPID, int* postShaderPID, bool force_update = false);

#ifdef EDITOR_CONTROLS
		void initImGui(HWND hwnd);
		void shutdownImGui();
		void renderImGui(Song* track, double position);
		LRESULT handleWindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

	private:
		int reloadShaderSource(const char* filename);

		bool compileAndDebugShader(const char* shader, const char* filename, bool kill_on_failure = true);

		enum PlayState {Playing, Paused};
		PlayState state;

		unsigned long lastFrameStart;
		unsigned long lastFrameStop;

		static const int shaderErrorBufferLength = 4096;
		static const int windowSize = 10;
		int timeHistory[windowSize] = {};

		bool shaderUpdatePending = false;
		int previousUpdateTime;

		double trackPosition;
		double trackEnd;

#ifdef EDITOR_CONTROLS
		bool imguiInitialized = false;
		bool showSeekBar = true;
		bool showControls = true;
		bool showStats = true;
#endif
	};
}
