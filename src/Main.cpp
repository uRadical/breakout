#include "Game.h"

#include <SDL3/SDL_main.h> // provides the platform entry point (e.g. WinMain on Windows)

int main(int /*argc*/, char** /*argv*/)
{
	Game game;
	if (game.Initialize())
	{
		game.RunLoop();
	}
	// SDL window, renderer, and SDL_Quit() are released automatically when
	// `game` goes out of scope (RAII) -- no explicit Shutdown() needed.
	return 0;
}
