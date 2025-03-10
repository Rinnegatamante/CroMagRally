// CRO-MAG RALLY ENTRY POINT
// (C) 2022 Iliyas Jorio
// This file is part of Cro-MagRally. https://github.com/jorio/cromagrally

#include <SDL.h>
#include "Pomme.h"
#include "PommeInit.h"
#include "PommeFiles.h"

#include <iostream>
#include <cstring>

#if __APPLE__
#include <libproc.h>
#include <unistd.h>
#endif

#ifdef __vita__
#include <unistd.h> 
#include <vitasdk.h>
extern "C" {
int _newlib_heap_size_user = 256 * 1024 * 1024;
void vglInitExtended(int legacy_pool_size, int width, int height, int ram_threshold, SceGxmMultisampleMode msaa);
};
int swap_interval = 1;
#endif

extern "C"
{
	#include "game.h"
	#include "version.h"

	SDL_Window* gSDLWindow = nullptr;
	FSSpec gDataSpec;
	CommandLineOptions gCommandLine;
	int gCurrentAntialiasingLevel;

#if 0 //_WIN32
	// Tell Windows graphics driver that we prefer running on a dedicated GPU if available
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
	__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
#endif

	int GameMain(void);
}

static fs::path FindGameData()
{
	fs::path dataPath;

#if __APPLE__
	char pathbuf[PROC_PIDPATHINFO_MAXSIZE];

	pid_t pid = getpid();
	int ret = proc_pidpath(pid, pathbuf, sizeof(pathbuf));
	if (ret <= 0)
	{
		throw std::runtime_error(std::string(__func__) + ": proc_pidpath failed: " + std::string(strerror(errno)));
	}

	dataPath = pathbuf;
	dataPath = dataPath.parent_path().parent_path() / "Resources";
#elif defined(__vita__)
	dataPath = "ux0:data/CroMagRally/Data";
#else
	dataPath = "Data";
#endif

	dataPath = dataPath.lexically_normal();

	// Set data spec -- Lets the game know where to find its asset files
	gDataSpec = Pomme::Files::HostPathToFSSpec(dataPath / "Skeletons");

	// Use application resource file
	auto applicationSpec = Pomme::Files::HostPathToFSSpec(dataPath / "System" / "Application");
	short resFileRefNum = FSpOpenResFile(&applicationSpec, fsRdPerm);

	if (resFileRefNum == -1)
	{
		throw std::runtime_error("Couldn't find the Data folder.");
	}

	UseResFile(resFileRefNum);

	return dataPath;
}

static void ParseCommandLine(int argc, char** argv)
{
	memset(&gCommandLine, 0, sizeof(gCommandLine));
	gCommandLine.vsync = 1;

	for (int i = 1; i < argc; i++)
	{
		std::string argument = argv[i];

		if (argument == "--track")
		{
			GAME_ASSERT_MESSAGE(i + 1 < argc, "practice track # unspecified");
			gCommandLine.bootToTrack = atoi(argv[i + 1]);
			i += 1;
		}
		else if (argument == "--car")
		{
			GAME_ASSERT_MESSAGE(i + 1 < argc, "car # unspecified");
			gCommandLine.car = atoi(argv[i + 1]);
			i += 1;
		}
		else if (argument == "--stats")
			gDebugMode = 1;
		else if (argument == "--no-vsync")
			gCommandLine.vsync = 0;
		else if (argument == "--vsync")
			gCommandLine.vsync = 1;
		else if (argument == "--adaptive-vsync")
			gCommandLine.vsync = -1;
#if 0
		else if (argument == "--fullscreen-resolution")
		{
			GAME_ASSERT_MESSAGE(i + 2 < argc, "fullscreen width & height unspecified");
			gCommandLine.fullscreenWidth = atoi(argv[i + 1]);
			gCommandLine.fullscreenHeight = atoi(argv[i + 2]);
			i += 2;
		}
		else if (argument == "--fullscreen-refresh-rate")
		{
			GAME_ASSERT_MESSAGE(i + 1 < argc, "fullscreen refresh rate unspecified");
			gCommandLine.fullscreenRefreshRate = atoi(argv[i + 1]);
			i += 1;
		}
#endif
	}
}

static void GetInitialWindowSize(int display, int& width, int& height)
{
	const float aspectRatio = 16.0f / 9.0f;
	const float screenCoverage = 2.0f / 3.0f;

	SDL_Rect displayBounds = { .x = 0, .y = 0, .w = 640, .h = 480 };
	SDL_GetDisplayUsableBounds(display, &displayBounds);

	if (displayBounds.w > displayBounds.h)
	{
		width	= displayBounds.h * screenCoverage * aspectRatio;
		height	= displayBounds.h * screenCoverage;
	}
	else
	{
		width	= displayBounds.w * screenCoverage;
		height	= displayBounds.w * screenCoverage / aspectRatio;
	}
}

static void Boot()
{
	// Start our "machine"
	Pomme::Init();

	// Load game prefs before starting
	InitDefaultPrefs();
	LoadPrefs();

retryVideo:
	// Initialize SDL video subsystem
	if (0 != SDL_Init(SDL_INIT_VIDEO))
	{
		throw std::runtime_error("Couldn't initialize SDL video subsystem.");
	}

	// Create window
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	gCurrentAntialiasingLevel = gGamePrefs.antialiasingLevel;
	if (gCurrentAntialiasingLevel != 0)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 1 << gCurrentAntialiasingLevel);
	}

	int display = gGamePrefs.monitorNum;
	if (display >= SDL_GetNumVideoDisplays())
	{
		display = 0;
	}

	int initialWidth = 640;
	int initialHeight = 480;
	GetInitialWindowSize(display, initialWidth, initialHeight);

	gSDLWindow = SDL_CreateWindow(
			"Cro-Mag Rally " PROJECT_VERSION,
			SDL_WINDOWPOS_UNDEFINED_DISPLAY(display),
			SDL_WINDOWPOS_UNDEFINED_DISPLAY(display),
			initialWidth,
			initialHeight,
			SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);

	if (!gSDLWindow)
	{
		if (gCurrentAntialiasingLevel != 0)
		{
			printf("Couldn't create SDL window with the requested MSAA level. Retrying without MSAA...\n");

			// retry without MSAA
			gGamePrefs.antialiasingLevel = 0;
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
			goto retryVideo;
		}
		else
		{
			throw std::runtime_error("Couldn't create SDL window.");
		}
	}

	// Find path to game data folder
	fs::path dataPath = FindGameData();

	// Init joystick subsystem
	{
		SDL_Init(SDL_INIT_GAMECONTROLLER);
		auto gamecontrollerdbPath8 = (dataPath / "System" / "gamecontrollerdb.txt").u8string();
		if (-1 == SDL_GameControllerAddMappingsFromFile((const char*)gamecontrollerdbPath8.c_str()))
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Cro-Mag Rally", "Couldn't load gamecontrollerdb.txt!", gSDLWindow);
		}
	}

#ifdef __vita__
	SDL_GL_SetSwapInterval(swap_interval);
#endif
}

static void Shutdown()
{
	Pomme::Shutdown();
	SDL_Quit();
}

int main(int argc, char** argv)
{
#ifdef __vita__
	SceAppUtilInitParam init_param;
	SceAppUtilBootParam boot_param;
	memset(&init_param, 0, sizeof(SceAppUtilInitParam));
	memset(&boot_param, 0, sizeof(SceAppUtilBootParam));
	sceAppUtilInit(&init_param, &boot_param);
	
	SceAppUtilAppEventParam eventParam;
	memset(&eventParam, 0, sizeof(SceAppUtilAppEventParam));
	sceAppUtilReceiveAppEvent(&eventParam);
	if (eventParam.type == 0x05) { // Game launched with 30 fps mode
		swap_interval = 2;
	}

	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
	scePowerSetGpuXbarClockFrequency(166);
	vglInitExtended(8 * 1024 * 1024, 960, 544, 8 * 1024 * 1024, SCE_GXM_MULTISAMPLE_4X);
	chdir("ux0:data");
#endif

	int				returnCode				= 0;
	std::string		finalErrorMessage		= "";
	bool			showFinalErrorMessage	= false;

	try
	{
		ParseCommandLine(argc, argv);
		Boot();
		returnCode = GameMain();
	}
	catch (Pomme::QuitRequest&)
	{
		// no-op, the game may throw this exception to shut us down cleanly
	}
#if !(_DEBUG)
	// In release builds, catch anything that might be thrown by GameMain
	// so we can show an error dialog to the user.
	catch (std::exception& ex)		// Last-resort catch
	{
		returnCode = 1;
		finalErrorMessage = ex.what();
		showFinalErrorMessage = true;
	}
	catch (...)						// Last-resort catch
	{
		returnCode = 1;
		finalErrorMessage = "unknown";
		showFinalErrorMessage = true;
	}
#endif

	Shutdown();

	if (showFinalErrorMessage)
	{
		std::cerr << "Uncaught exception: " << finalErrorMessage << "\n";
		SDL_ShowSimpleMessageBox(0, "Cro-Mag Rally", finalErrorMessage.c_str(), nullptr);
	}

	return returnCode;
}
