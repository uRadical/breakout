#pragma once
#include <SDL3/SDL.h>

#include "Audio.h"
#include "Physics.h" // Vector2 and the pure collision/deflection maths

#include <memory>
#include <vector>

// A single breakable brick: its rectangle, colour, and whether it's still
// in play (a destroyed brick stays in the vector but stops drawing/colliding).
struct Brick
{
	SDL_FRect rect{};
	Uint8 r = 0, g = 0, b = 0;
	int row = 0; // which row (0 = top); drives the break sound's pitch
	bool alive = true;
};

// Game owns the SDL window/renderer and runs the main loop.
class Game
{
public:
	Game() = default;

	// Set up SDL and the window/renderer. Returns false on failure.
	bool Initialize();
	// Run the game loop until the player quits or loses.
	void RunLoop();

private:
	// Helper functions for the game loop.
	void ProcessInput();
	void UpdateGame();
	void GenerateOutput();

	// Re-serve the ball and recentre the paddle (after a life loss or on a new
	// level). The serve speed scales with the current level.
	void ResetBall();
	// (Re)build the full brick wall for the current level.
	void BuildBricks();
	// Start a brand-new game: level 1, full lives, zero score, fresh wall, serve.
	void ResetRound();

	// RAII guard for SDL itself: SDL_Quit() runs automatically on destruction.
	struct SDLContext
	{
		bool initialized = false;
		~SDLContext()
		{
			if (initialized)
			{
				SDL_Quit();
			}
		}
		SDLContext() = default;
		SDLContext(const SDLContext&) = delete;
		SDLContext& operator=(const SDLContext&) = delete;
	};

	// Custom deleters let unique_ptr release SDL resources automatically.
	struct WindowDeleter
	{
		void operator()(SDL_Window* w) const { SDL_DestroyWindow(w); }
	};
	struct RendererDeleter
	{
		void operator()(SDL_Renderer* r) const { SDL_DestroyRenderer(r); }
	};

	// Declaration order is intentional: members are destroyed bottom-to-top,
	// so the renderer is released before the window, and SDL_Quit (mSDL) runs
	// last of all.
	SDLContext mSDL;
	std::unique_ptr<SDL_Window, WindowDeleter> mWindow;
	std::unique_ptr<SDL_Renderer, RendererDeleter> mRenderer;

	// The single game state. The simulation only advances while Playing; every
	// other state freezes it and shows a corresponding overlay box.
	enum class State : Uint8
	{
		Start,    // title screen, waiting for the first key
		Playing,  // ball in motion
		Paused,   // space-bar pause
		LifeLost, // ball dropped, lives remain, waiting to continue
		Lost,     // out of lives
	};

	// Number of ticks since the start of the game.
	Uint64 mTicksCount = 0;
	// Game should continue to run.
	bool mIsRunning = true;
	// Current game state (see above).
	State mState = State::Start;
	// Remaining lives, shown as hearts. The game ends when this hits zero.
	int mLives = 0;
	// Current level (1-based); each level serves the ball faster.
	int mLevel = 1;

	// Breakout-specific state.
	int mPaddleDir = 0;         // Direction of paddle movement (-1, 0, +1)
	Vector2 mPaddlePos;         // Position of paddle (centre x, top y)
	Vector2 mBallPos;           // Position of ball (centre)
	Vector2 mBallVel;           // Velocity of ball
	std::vector<Brick> mBricks; // The wall of breakable bricks.
	int mScore = 0;             // Points this game (one tally per brick broken).
	Audio mAudio;               // Procedural sound effects.
};
