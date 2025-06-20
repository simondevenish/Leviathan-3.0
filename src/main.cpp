// custom build and feature flags
#ifdef DEBUG
	#define OPENGL_DEBUG        1
	#define FULLSCREEN          0
	#define DESPERATE           0
	#define BREAK_COMPATIBILITY 0
#else
	#define OPENGL_DEBUG        0
	#define FULLSCREEN          1
	#define DESPERATE           0
	#define BREAK_COMPATIBILITY 0
#endif

#define POST_PASS    1
#define USE_MIPMAPS  1
#define USE_AUDIO    1
#define NO_UNIFORMS  0

#include "definitions.h"
#if OPENGL_DEBUG
	#include "debug.h"
#endif

#include "glext.h"

#ifdef EDITOR_CONTROLS
#include "editor.h"
#include "song.h"
#endif
#pragma data_seg(".shader")
#include "shaders/fragment.inl"
#if POST_PASS
	#pragma data_seg(".shader")
	#include "shaders/post.inl"
#endif

#pragma data_seg(".pids")
// static allocation saves a few bytes
static int pidMain;
static int pidPost;
// static HDC hDC;

#ifdef EDITOR_CONTROLS
// Window procedure for ImGui message handling
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static Leviathan::Editor* g_editor = nullptr;
#endif

#ifndef EDITOR_CONTROLS
#pragma code_seg(".main")
void entrypoint(void)
#else
int __cdecl main(int argc, char* argv[])
#endif
{
	// initialize window
	#if FULLSCREEN
		ChangeDisplaySettings(&screenSettings, CDS_FULLSCREEN);
		ShowCursor(0);
		const HDC hDC = GetDC(CreateWindow(L"static", 0, WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, 0, 0, 0, 0, 0, 0));
	#else
		#ifdef EDITOR_CONTROLS
			// Register window class for proper message handling
			WNDCLASSW wc = {0};
			wc.lpfnWndProc = WindowProc;
			wc.hInstance = GetModuleHandle(NULL);
			wc.lpszClassName = L"LeviathanEditor";
			wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
			RegisterClassW(&wc);
			
			HWND window = CreateWindowW(L"LeviathanEditor", L"Leviathan Editor", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
				CW_USEDEFAULT, CW_USEDEFAULT, XRES, YRES, 0, 0, GetModuleHandle(NULL), 0);
			HDC hDC = GetDC(window);
		#else
			// you can create a pseudo fullscreen window by similarly enabling the WS_MAXIMIZE flag as above
			// in which case you can replace the resolution parameters with 0s and save a couple bytes
			// this only works if the resolution is set to the display device's native resolution
			HDC hDC = GetDC(CreateWindow(L"static", 0, WS_POPUP | WS_VISIBLE, 0, 0, XRES, YRES, 0, 0, 0, 0));
		#endif
	#endif

	// initalize opengl context
	SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd);
	wglMakeCurrent(hDC, wglCreateContext(hDC));
	
	// create and compile shader programs
	pidMain = ((PFNGLCREATESHADERPROGRAMVPROC)wglGetProcAddress("glCreateShaderProgramv"))(GL_FRAGMENT_SHADER, 1, &fragment_frag);
	#if POST_PASS
		pidPost = ((PFNGLCREATESHADERPROGRAMVPROC)wglGetProcAddress("glCreateShaderProgramv"))(GL_FRAGMENT_SHADER, 1, &post_frag);
	#endif

	// initialize sound
	#ifndef EDITOR_CONTROLS
		#if USE_AUDIO
			CreateThread(0, 0, (LPTHREAD_START_ROUTINE)_4klang_render, lpSoundBuffer, 0, 0);
			waveOutOpen(&hWaveOut, WAVE_MAPPER, &WaveFMT, NULL, 0, CALLBACK_NULL);
			waveOutPrepareHeader(hWaveOut, &WaveHDR, sizeof(WaveHDR));
			waveOutWrite(hWaveOut, &WaveHDR, sizeof(WaveHDR));
		#endif
	#else
		Leviathan::Editor editor = Leviathan::Editor();
		g_editor = &editor;
		editor.updateShaders(&pidMain, &pidPost, true);

		// Initialize ImGui
		editor.initImGui(window);

		// absolute path always works here
		// relative path works only when not ran from visual studio directly
		Leviathan::Song track(L"audio.wav");
		track.play();
		double position = 0.0;
	#endif

	// main loop
	do
	{
		#ifdef EDITOR_CONTROLS
			editor.beginFrame(timeGetTime());
			
			// Handle window messages for ImGui
			MSG msg;
			while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				if (msg.message == WM_QUIT)
					goto exit_loop;
			}
		#endif

		#if !(DESPERATE)
			// do minimal message handling so windows doesn't kill your application
			// not always strictly necessary but increases compatibility and reliability a lot
			// normally you'd pass an msg struct as the first argument but it's just an
			// output parameter and the implementation presumably does a NULL check
			#ifndef EDITOR_CONTROLS
				PeekMessage(0, 0, 0, 0, PM_REMOVE);
			#endif
		#endif

		// render with the primary shader
		((PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram"))(pidMain);
		#ifndef EDITOR_CONTROLS
			// if you don't have an audio system figure some other way to pass time to your shader
			#if USE_AUDIO
				waveOutGetPosition(hWaveOut, &MMTime, sizeof(MMTIME));
				// it is possible to upload your vars as vertex color attribute (gl_Color) to save one function import
				#if NO_UNIFORMS
					glColor3ui(MMTime.u.sample, 0, 0);
				#else
					// remember to divide your shader time variable with the SAMPLE_RATE (44100 with 4klang)
					((PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i"))(0, MMTime.u.sample);
				#endif
			#endif
		#else
			position = track.getTime();
			((PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i"))(0, (static_cast<int>(position*44100.0)));
		#endif
		glRects(-1, -1, 1, 1);

		// render "post process" using the opengl backbuffer
		#if POST_PASS
			glBindTexture(GL_TEXTURE_2D, 1);
			#if USE_MIPMAPS
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, XRES, YRES, 0);
				((PFNGLGENERATEMIPMAPPROC)wglGetProcAddress("glGenerateMipmap"))(GL_TEXTURE_2D);
			#else
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, XRES, YRES, 0);
			#endif
			((PFNGLACTIVETEXTUREPROC)wglGetProcAddress("glActiveTexture"))(GL_TEXTURE0);
			((PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram"))(pidPost);
			((PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i"))(0, 0);
			glRects(-1, -1, 1, 1);
		#endif

		// Render ImGui overlay
		#ifdef EDITOR_CONTROLS
			editor.renderImGui(&track, position);
		#endif

		SwapBuffers(hDC);

		// handle functionality of the editor
		#ifdef EDITOR_CONTROLS
			editor.endFrame(timeGetTime());
			position = editor.handleEvents(&track, position);
			editor.printFrameStatistics();
			editor.updateShaders(&pidMain, &pidPost);
		#endif

	} while(!GetAsyncKeyState(VK_ESCAPE)
		#if USE_AUDIO
			&& MMTime.u.sample < MAX_SAMPLES
		#endif
	);

	#ifdef EDITOR_CONTROLS
	exit_loop:
	#endif
	ExitProcess(0);
}

#ifdef EDITOR_CONTROLS
// Window procedure for handling ImGui messages
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (g_editor)
	{
		LRESULT result = g_editor->handleWindowMessage(hWnd, message, wParam, lParam);
		if (result != 0)
			return result;
	}

	switch (message)
	{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
#endif
