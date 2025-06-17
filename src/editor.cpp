#include "editor.h"
#include "song.h"

#include "stdio.h"
#include "windows.h"
#include "GL/gl.h"
#include "glext.h"

#ifdef EDITOR_CONTROLS
// Include additional Windows headers for ImGui
#include <windowsx.h>
#include <tchar.h>
#endif

using namespace Leviathan;

#define USE_MESSAGEBOX 0

Editor::Editor() : lastFrameStart(0), lastFrameStop(0), trackPosition(0.0), trackEnd(0.0), state(Playing)
{
	printf("Editor opened...\n");
}

#ifdef EDITOR_CONTROLS
Editor::~Editor()
{
	if (imguiInitialized)
	{
		shutdownImGui();
	}
}

void Editor::initImGui(HWND hwnd)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplOpenGL3_Init("#version 130");

	imguiInitialized = true;
	printf("ImGui initialized\n");
}

void Editor::shutdownImGui()
{
	if (imguiInitialized)
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		imguiInitialized = false;
	}
}

void Editor::renderImGui(Song* track, double position)
{
	if (!imguiInitialized) return;

	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Create the main editor overlay
	if (showControls)
	{
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(400, 120), ImGuiCond_FirstUseEver);
		
		if (ImGui::Begin("Leviathan Editor", &showControls, ImGuiWindowFlags_AlwaysAutoResize))
		{
			// Play/Pause controls
			if (state == Playing)
			{
				if (ImGui::Button("Pause"))
				{
					state = Paused;
					track->pause();
				}
			}
			else
			{
				if (ImGui::Button("Play"))
				{
					state = Playing;
					track->play();
				}
			}
			
			ImGui::SameLine();
			
			// Time display
			int minutes = (int)(position / 60.0);
			int seconds = (int)position % 60;
			int percentage = trackEnd > 0 ? (int)(100.0 * position / trackEnd) : 0;
			ImGui::Text("Time: %02d:%02d (%d%%)", minutes, seconds, percentage);

			// Seekbar
			if (trackEnd > 0)
			{
				float seekValue = (float)(position / trackEnd);
				if (ImGui::SliderFloat("Timeline", &seekValue, 0.0f, 1.0f, "%.2f"))
				{
					double newPosition = seekValue * trackEnd;
					track->seek(newPosition);
				}
			}

			// Quick seek buttons
			if (ImGui::Button("-10s"))
			{
				double newPos = position - 10.0;
				if (newPos < 0) newPos = 0;
				track->seek(newPos);
			}
			ImGui::SameLine();
			if (ImGui::Button("-1s"))
			{
				double newPos = position - 1.0;
				if (newPos < 0) newPos = 0;
				track->seek(newPos);
			}
			ImGui::SameLine();
			if (ImGui::Button("+1s"))
			{
				track->seek(position + 1.0);
			}
			ImGui::SameLine();
			if (ImGui::Button("+10s"))
			{
				track->seek(position + 10.0);
			}

			// Shader reload button
			ImGui::SameLine();
			if (ImGui::Button("Reload Shaders"))
			{
				shaderUpdatePending = true;
			}
		}
		ImGui::End();
	}

	// Performance stats window
	if (showStats)
	{
		ImGui::SetNextWindowPos(ImVec2(10, 150), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(250, 100), ImGuiCond_FirstUseEver);
		
		if (ImGui::Begin("Performance", &showStats, ImGuiWindowFlags_AlwaysAutoResize))
		{
			const int frameTime = lastFrameStop - lastFrameStart;
			
			// Calculate average FPS
			float fps = 0.0f;
			for (int i = 0; i < windowSize; ++i)
			{
				if (timeHistory[i] > 0)
					fps += 1000.0f / static_cast<float>(timeHistory[i]);
			}
			fps /= static_cast<float>(windowSize);

			ImGui::Text("Frame time: %d ms", frameTime);
			ImGui::Text("FPS: %.1f", fps);
			ImGui::Text("State: %s", state == Playing ? "Playing" : "Paused");
		}
		ImGui::End();
	}

	// Menu bar for toggling windows
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Windows"))
		{
			ImGui::MenuItem("Controls", nullptr, &showControls);
			ImGui::MenuItem("Performance", nullptr, &showStats);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	// Render ImGui
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

LRESULT Editor::handleWindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (imguiInitialized)
	{
		return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
	}
	return 0;
}

#else
Editor::~Editor() {}
#endif

void Editor::beginFrame(const unsigned long time)
{
	lastFrameStart = time;
}

void Editor::endFrame(const unsigned long time)
{
	lastFrameStop = time;
}

void Editor::printFrameStatistics()
{
	const int frameTime = lastFrameStop - lastFrameStart;

	// calculate average fps over 'windowSize' of frames
	float fps = 0.0f;
	for (int i = 0; i < windowSize - 1; ++i)
	{
		timeHistory[i] = timeHistory[i + 1];
		fps += 1.0f / static_cast<float>(timeHistory[i]);
	}
	timeHistory[windowSize - 1] = frameTime;
	fps += 1.0f / static_cast<float>(frameTime);
	fps *= 1000.0f / static_cast<float>(windowSize);

	printf("%s: %0.2i:%0.2i (%i%%), frame duration: %i ms (running fps average: %2.2f) \r",
		state == Playing ? "Playing" : " Paused",
		// assuming y'all won't be making intros more than an hour long
		int(trackPosition/60.0), int(trackPosition) % 60, int(100.0f*trackPosition/trackEnd),
		frameTime, fps);
}

double Editor::handleEvents(Leviathan::Song* track, double position)
{
	if (GetAsyncKeyState(VK_MENU))
	{
		double seek = 0.0;
		if (GetAsyncKeyState(VK_DOWN))
		{
			state = Paused;
			track->pause();
		}
		if (GetAsyncKeyState(VK_UP))
		{
			state = Playing;
			track->play();
		}
		if (GetAsyncKeyState(VK_RIGHT) && !GetAsyncKeyState(VK_SHIFT)) seek += 1.0;
		if (GetAsyncKeyState(VK_LEFT)  && !GetAsyncKeyState(VK_SHIFT)) seek -= 1.0;
		if (GetAsyncKeyState(VK_RIGHT) && GetAsyncKeyState(VK_SHIFT))  seek += 0.1;
		if (GetAsyncKeyState(VK_LEFT)  && GetAsyncKeyState(VK_SHIFT))  seek -= 0.1;
		if (position + seek != position)
		{
			position += seek;
			track->seek(position);
		}
	}

	if (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState('S'))
		shaderUpdatePending = true;

	trackPosition = position;
	trackEnd = track->getLength();

	return position;
}

void Editor::updateShaders(int* mainShaderPID, int* postShaderPID, bool force_update)
{
	if (shaderUpdatePending || force_update)
	{
		// make sure the file has finished writing to disk
		if (timeGetTime() - previousUpdateTime > 200)
		{
			// only way i can think of to clear the line without "status line" residue
			printf("Refreshing shaders...                                                   \n");

			Sleep(100);
			int newPID = reloadShaderSource("src/shaders/fragment.frag");
			if (newPID > 0)
				*mainShaderPID = newPID;

			newPID = reloadShaderSource("src/shaders/post.frag");
			if (newPID > 0)
				*postShaderPID = newPID;
		}

		previousUpdateTime = timeGetTime();
		shaderUpdatePending = false;
	}
}

int Editor::reloadShaderSource(const char* filename)
{
	long inputSize = 0;
	// we're of course opening a text file, but should be opened in binary ('b')
	// longer shaders are known to cause problems by producing garbage input when read
	FILE* file = fopen(filename, "rb");

	if (file)
	{
		fseek(file, 0, SEEK_END);
		inputSize = ftell(file);
		rewind(file);

		char* shaderString = static_cast<char*>(calloc(inputSize + 1, sizeof(char)));
		fread(shaderString, sizeof(char), inputSize, file);
		fclose(file);

		// just to be sure...
		shaderString[inputSize] = '\0';

		if (!compileAndDebugShader(shaderString, filename, false))
		{
			free(shaderString);
			// return an invalid PID value if compilation fails
			return -1;
		}
		
		int pid = ((PFNGLCREATESHADERPROGRAMVPROC)wglGetProcAddress("glCreateShaderProgramv"))(GL_FRAGMENT_SHADER, 1, &shaderString);
		free(shaderString);

		printf("Loaded shader from \"%s\"\n", filename);
		return pid;
	}
	else
	{
		printf("Input shader file at \"%s\" not found, shader not reloaded\n", filename);
		return -1;
	}
}

bool Editor::compileAndDebugShader(const char* shader, const char* filename, bool kill_on_failure)
{
	// try and compile the shader 
	int result = 0;
	const int debugid = ((PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader"))(GL_FRAGMENT_SHADER);
	((PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource"))(debugid, 1, &shader, 0);
	((PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader"))(debugid);

	// get compile result
	((PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv"))(debugid, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE)
	{
		// display compile log on failure
		static char errorBuffer[shaderErrorBufferLength];
		((PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog"))(debugid, shaderErrorBufferLength-1, NULL, static_cast<char*>(errorBuffer));
		
		#if USE_MESSAGEBOX
			MessageBox(NULL, errorBuffer, "", 0x00000000L);
		#endif
		printf("Compilation errors in %s:\n\n %s\n", filename, errorBuffer);

		if (kill_on_failure)
		{
			ExitProcess(0);
		}
		else
		{
			return false;
		}
	}
	else
	{
		((PFNGLDELETESHADERPROC)wglGetProcAddress("glDeleteShader"))(debugid);
		return true;
	}
}
