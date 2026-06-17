#include "Game.h"

#include "Draw.h" // stateless 2D drawing helpers
#include "Icon.h" // embedded app icon (uRadical brand colours)

#include <algorithm> // std::clamp, std::ranges::any_of
#include <numbers>   // std::numbers::pi_v

// --- Screen & layout -------------------------------------------------------
constexpr float screenW = 1024.0f;
constexpr float screenH = 768.0f;
constexpr float thickness = 15.0f; // wall and paddle thickness
constexpr float paddleW = 100.0f;

// --- Timing & motion -------------------------------------------------------
constexpr Uint64 frameMs = 16;           // frame cap (~60 fps)
constexpr float maxDeltaTime = 0.05f;    // clamp after a stall so the ball can't skip
constexpr float paddleSpeed = 300.0f;    // paddle movement, px/s
constexpr float ballStartVelX = -200.0f; // initial (level 1) ball velocity
constexpr float ballStartVelY = -235.0f; // negative = upward
constexpr float paddleHitBand = 5.0f;    // vertical tolerance above the paddle top
constexpr float levelSpeedStep = 0.12f;  // ball speed gained per level
constexpr float maxSpeedFactor = 2.2f;   // cap so the ball can't tunnel through objects
// Largest rebound angle off the paddle, measured from straight up. Hitting the
// centre sends the ball vertical; the edges send it off at this angle, letting
// the player steer. pi/3 = 60 degrees.
constexpr float maxBounceAngle = std::numbers::pi_v<float> / 3.0f;

// --- Brick wall ------------------------------------------------------------
// Eight rows, coloured top-to-bottom to match the app icon's brand gradient
// (sky blue -> purple -> pink -> gold), two rows per colour.
constexpr int brickCols = 11;
constexpr int brickRows = 8;
constexpr int rowsPerColour = 2;
constexpr float brickH = 26.0f;    // Height of each brick
constexpr float brickGap = 6.0f;   // Gap between bricks (and to the side walls)
constexpr float brickTop = 70.0f;  // Y of the first row's top edge
constexpr int pointsPerBrick = 10; // Score awarded for breaking one brick
constexpr int startingLives = 3;   // Lives at the start of a game (shown as hearts)

struct RGB
{
	Uint8 r, g, b;
};
constexpr RGB brickPalette[] = {
    {93, 177, 255},  // Sky blue  (#5DB1FF)
    {159, 114, 225}, // Purple    (#9F72E1)
    {246, 89, 168},  // Pink      (#F659A8)
    {225, 198, 49},  // Gold      (#E1C631)
};

bool Game::Initialize()
{
	// Initialize SDL (SDL3 returns true on success)
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
		return false;
	}
	mSDL.initialized = true; // ensures SDL_Quit() on destruction

	// Create an SDL Window (SDL3 drops the x/y position args)
	mWindow.reset(SDL_CreateWindow(
	    "Breakout",                // Window title
	    static_cast<int>(screenW), // Width of window
	    static_cast<int>(screenH), // Height of window
	    0                          // Flags (0 for no flags set)
	    ));

	if (!mWindow)
	{
		SDL_Log("Failed to create window: %s", SDL_GetError());
		return false;
	}

	// Set the window/Dock icon from the embedded BMP (uRadical brand colours).
	// Loaded from memory so the executable stays self-contained. A failure here
	// is non-fatal: we just keep the default icon.
	if (SDL_Surface* icon =
	        SDL_LoadBMP_IO(SDL_IOFromConstMem(kIconBmp, kIconBmp_len), true /* closeio */))
	{
		SDL_SetWindowIcon(mWindow.get(), icon);
		SDL_DestroySurface(icon);
	}

	// Create SDL renderer (SDL3 takes a driver name, not index/flags)
	mRenderer.reset(SDL_CreateRenderer(mWindow.get(), nullptr));

	if (!mRenderer)
	{
		SDL_Log("Failed to create renderer: %s", SDL_GetError());
		return false;
	}

	// Enable vsync (was a SDL_CreateRenderer flag in SDL2)
	SDL_SetRenderVSync(mRenderer.get(), 1);

	// Sound effects are optional: if audio can't initialize, the game runs silent.
	mAudio.Initialize(brickRows);

	ResetRound();
	mState = State::Start; // show the title screen until the first key press
	return true;
}

void Game::ResetBall()
{
	// Paddle sits on the bottom edge: x is its centre, y is its (fixed) top.
	mPaddlePos = {screenW / 2.0f, screenH - thickness * 2.0f};
	mBallPos = {screenW / 2.0f, screenH / 2.0f};
	// Launch upward (negative y) so the ball heads away from the bottom paddle
	// first. Higher levels serve faster, up to a cap that keeps the ball from
	// moving far enough per frame to tunnel through objects.
	float speedFactor = 1.0f + static_cast<float>(mLevel - 1) * levelSpeedStep;
	if (speedFactor > maxSpeedFactor)
	{
		speedFactor = maxSpeedFactor;
	}
	mBallVel = Vector2{ballStartVelX, ballStartVelY} * speedFactor;
}

void Game::BuildBricks()
{
	// Build the brick wall, spanning the play area between the side walls.
	mBricks.clear();
	const float playLeft = thickness + brickGap;
	const float playWidth = screenW - 2.0f * (thickness + brickGap);
	const float brickW =
	    (playWidth - static_cast<float>(brickCols - 1) * brickGap) / static_cast<float>(brickCols);
	for (int row = 0; row < brickRows; ++row)
	{
		for (int col = 0; col < brickCols; ++col)
		{
			Brick brick;
			brick.rect.x = playLeft + static_cast<float>(col) * (brickW + brickGap);
			brick.rect.y = brickTop + static_cast<float>(row) * (brickH + brickGap);
			brick.rect.w = brickW;
			brick.rect.h = brickH;
			brick.row = row;
			const RGB& colour = brickPalette[row / rowsPerColour];
			brick.r = colour.r;
			brick.g = colour.g;
			brick.b = colour.b;
			mBricks.push_back(brick);
		}
	}
}

void Game::ResetRound()
{
	// A fresh game, starting from level 1. The caller sets mState (Start on
	// launch, Playing on restart).
	mLevel = 1;
	mScore = 0;
	mLives = startingLives;
	BuildBricks();
	ResetBall();
}

void Game::RunLoop()
{
	while (mIsRunning)
	{
		ProcessInput();
		UpdateGame();
		GenerateOutput();
	}
}

void Game::ProcessInput()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		// If we get an SDL_QUIT event, end loop
		case SDL_EVENT_QUIT:
			mIsRunning = false;
			break;
		case SDL_EVENT_KEY_DOWN:
		{
			const SDL_Scancode key = event.key.scancode;
			switch (mState)
			{
			case State::Start: // any key (except Escape) begins play
				if (key != SDL_SCANCODE_ESCAPE)
				{
					mState = State::Playing;
				}
				break;
			case State::Playing: // space bar pauses
				if (key == SDL_SCANCODE_SPACE)
				{
					mState = State::Paused;
				}
				break;
			case State::Paused: // space bar resumes
				if (key == SDL_SCANCODE_SPACE)
				{
					mState = State::Playing;
				}
				break;
			case State::LifeLost: // any key re-serves and resumes where it left off
				if (key != SDL_SCANCODE_ESCAPE)
				{
					ResetBall();
					mState = State::Playing;
				}
				break;
			case State::Lost: // Y starts a new game, N quits
				if (key == SDL_SCANCODE_Y)
				{
					ResetRound();
					mState = State::Playing;
				}
				else if (key == SDL_SCANCODE_N)
				{
					mIsRunning = false;
				}
				break;
			}
			break;
		}
		default:
			break;
		}
	}

	// Get state of keyboard (SDL3 returns const bool*)
	const bool* state = SDL_GetKeyboardState(nullptr);
	// If escape is pressed, also end loop
	if (state[SDL_SCANCODE_ESCAPE])
	{
		mIsRunning = false;
	}

	// Update paddle direction based on A/D (or Left/Right arrow) keys
	mPaddleDir = 0;
	if (state[SDL_SCANCODE_A] || state[SDL_SCANCODE_LEFT])
	{
		mPaddleDir -= 1;
	}
	if (state[SDL_SCANCODE_D] || state[SDL_SCANCODE_RIGHT])
	{
		mPaddleDir += 1;
	}
}

void Game::UpdateGame()
{
	// Cap the frame rate by sleeping (not busy-waiting) until the next frame is
	// due. Vsync also paces presentation, but this keeps the simulation step at
	// a steady ~60 fps regardless of the display's refresh rate.
	const Uint64 frameEnd = mTicksCount + frameMs;
	const Uint64 now = SDL_GetTicks();
	if (now < frameEnd)
	{
		SDL_Delay(static_cast<Uint32>(frameEnd - now));
	}

	// Delta time is the difference in ticks from last frame (in seconds).
	float deltaTime = static_cast<float>(SDL_GetTicks() - mTicksCount) / 1000.0f;

	// Clamp so a long stall (e.g. dragging the window) can't teleport the ball.
	if (deltaTime > maxDeltaTime)
	{
		deltaTime = maxDeltaTime;
	}

	// Update tick counts (for next frame)
	mTicksCount = SDL_GetTicks();

	// The simulation only advances while playing; every other state freezes it
	// and GenerateOutput draws the appropriate overlay. (mTicksCount is still
	// advanced above, so resuming doesn't produce a large delta-time jump.)
	if (mState != State::Playing)
	{
		return;
	}

	// Update paddle position based on direction (now horizontal)
	if (mPaddleDir != 0)
	{
		mPaddlePos.x += static_cast<float>(mPaddleDir) * paddleSpeed * deltaTime;
		// Keep the paddle within the side walls.
		const float minX = paddleW / 2.0f + thickness;
		const float maxX = screenW - paddleW / 2.0f - thickness;
		mPaddlePos.x = std::clamp(mPaddlePos.x, minX, maxX);
	}

	// Update ball position based on ball velocity
	mBallPos += mBallVel * deltaTime;

	// Bounce if needed
	// Did we intersect with the paddle?
	const float diff = SDL_fabsf(mPaddlePos.x - mBallPos.x);
	if (
	    // Our x-difference is small enough
	    diff <= paddleW / 2.0f &&
	    // We are in the correct y-position (just above the paddle's top)
	    mBallPos.y >= (mPaddlePos.y - paddleHitBand) &&
	    mBallPos.y <= (mPaddlePos.y + paddleHitBand) &&
	    // The ball is moving down
	    mBallVel.y > 0.0f)
	{
		// Deflect based on where the ball struck the paddle (centre -> straight
		// up, edges -> angled), so the player aims with the paddle.
		mBallVel = PaddleDeflection(mBallVel, mBallPos.x, mPaddlePos.x, paddleW, maxBounceAngle);
		mAudio.Play(Audio::Sound::Paddle);
	}
	// Did the ball go off the bottom of the screen? Lose a life; if that was the
	// last one it's game over, otherwise pause for the player to continue.
	else if (mBallPos.y >= screenH)
	{
		mLives -= 1;
		mAudio.Play(Audio::Sound::Lose);
		mState = (mLives <= 0) ? State::Lost : State::LifeLost;
	}
	// Did the ball collide with the top wall?
	else if (mBallPos.y <= thickness && mBallVel.y < 0.0f)
	{
		mBallVel.y *= -1.0f;
		mAudio.Play(Audio::Sound::Wall);
	}

	// Did the ball collide with either side wall (moving toward it)?
	if ((mBallPos.x <= thickness && mBallVel.x < 0.0f) ||
	    (mBallPos.x >= (screenW - thickness) && mBallVel.x > 0.0f))
	{
		mBallVel.x *= -1.0f;
		mAudio.Play(Audio::Sound::Wall);
	}

	// Did the ball hit a brick? Reflect off the nearest face, break it, and stop
	// after one hit per frame to keep the bounce unambiguous.
	const float ballR = thickness / 2.0f;
	for (Brick& brick : mBricks)
	{
		if (!brick.alive)
		{
			continue;
		}
		const Rect rect{brick.rect.x, brick.rect.y, brick.rect.w, brick.rect.h};
		const BounceAxis axis = BrickBounce(mBallPos, ballR, rect);
		if (axis == BounceAxis::None)
		{
			continue;
		}
		if (axis == BounceAxis::Horizontal)
		{
			mBallVel.x *= -1.0f;
		}
		else
		{
			mBallVel.y *= -1.0f;
		}
		brick.alive = false;
		mScore += pointsPerBrick;
		mAudio.PlayBrick(brick.row);
		break;
	}

	// Clearing every brick advances to the next (faster) level: keep the score
	// and lives, rebuild the wall, and re-serve.
	const bool anyAlive = std::ranges::any_of(mBricks, [](const Brick& b)
	                                          { return b.alive; });
	if (!anyAlive)
	{
		++mLevel;
		BuildBricks();
		ResetBall();
		mAudio.Play(Audio::Sound::Win); // level-clear jingle
	}
}

void Game::GenerateOutput()
{
	SDL_Renderer* renderer = mRenderer.get();

	// Set draw color to dark navy (#0C1A50)
	SDL_SetRenderDrawColor(renderer, 12, 26, 80, 255);

	// Clear back buffer
	SDL_RenderClear(renderer);

	// Draw walls in purple (#9F72E1)
	SDL_SetRenderDrawColor(renderer, 159, 114, 225, 255);

	// Draw top wall (SDL3 render rects are SDL_FRect / floats)
	SDL_FRect wall{
	    0.0f,     // Top left x
	    0.0f,     // Top left y
	    screenW,  // Width
	    thickness // Height
	};
	SDL_RenderFillRect(renderer, &wall);

	// Draw left wall
	wall.x = 0.0f;
	wall.y = 0.0f;
	wall.w = thickness;
	wall.h = screenH;
	SDL_RenderFillRect(renderer, &wall);

	// Draw right wall
	wall.x = screenW - thickness;
	SDL_RenderFillRect(renderer, &wall);

	// Draw the brick wall (rounded corners, like the icon). Destroyed bricks
	// are skipped.
	for (const Brick& brick : mBricks)
	{
		if (!brick.alive)
		{
			continue;
		}
		SDL_SetRenderDrawColor(renderer, brick.r, brick.g, brick.b, 255);
		DrawRoundedRect(renderer, brick.rect.x, brick.rect.y, brick.rect.w, brick.rect.h,
		                brick.rect.h * 0.28f); // corner radius (matches the icon)
	}

	// Draw paddle in light blue (#5DB1FF) with rounded corners
	SDL_SetRenderDrawColor(renderer, 93, 177, 255, 255);
	DrawRoundedRect(
	    renderer,
	    mPaddlePos.x - paddleW / 2.0f,
	    mPaddlePos.y,
	    paddleW,
	    thickness,
	    thickness / 2.5f); // corner radius

	// Draw ball in gold (#E1C631)
	SDL_SetRenderDrawColor(renderer, 225, 198, 49, 255);
	DrawFilledCircle(renderer, mBallPos.x, mBallPos.y, thickness / 2.0f);

	// Score tally, right-aligned just below the top wall. DrawCenteredText takes
	// a centre, so derive one that keeps the text's right edge fixed near the
	// right wall as the digit count grows.
	char score[32];
	SDL_snprintf(score, sizeof(score), "SCORE %d", mScore);
	const float scoreScale = 2.0f;
	const float scoreWidth =
	    static_cast<float>(SDL_strlen(score)) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scoreScale;
	const float scoreRight = screenW - thickness - 12.0f; // small margin from the right wall
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	DrawCenteredText(renderer, scoreRight - scoreWidth / 2.0f, thickness + 28.0f, score, scoreScale);

	// Current level, centred along the top between the hearts and the score.
	char level[24];
	SDL_snprintf(level, sizeof(level), "LEVEL %d", mLevel);
	DrawCenteredText(renderer, screenW / 2.0f, thickness + 28.0f, level, 2.0f);

	// Lives as hearts, top-left. Remaining lives are pink (#F659A8); spent slots
	// are drawn dim so the row keeps a constant width.
	const float heartSize = 26.0f;
	const float heartSpacing = 32.0f;
	const float heartCy = thickness + 24.0f;
	for (int i = 0; i < startingLives; ++i)
	{
		const float heartCx = thickness + 22.0f + static_cast<float>(i) * heartSpacing;
		if (i < mLives)
		{
			SDL_SetRenderDrawColor(renderer, 246, 89, 168, 255); // pink: a life left
		}
		else
		{
			SDL_SetRenderDrawColor(renderer, 60, 72, 120, 255); // dim: a spent life
		}
		DrawHeart(renderer, heartCx, heartCy, heartSize);
	}

	// Centre of the screen, where all overlay message boxes are drawn.
	const float midX = screenW / 2.0f;
	const float midY = screenH / 2.0f;

	// State overlay: one message box per non-playing state.
	switch (mState)
	{
	case State::Start:
		DrawMessageBox(renderer, midX, midY, "PRESS ANY KEY TO PLAY", 2.0f, nullptr, 0.0f);
		break;
	case State::Lost:
		DrawMessageBox(renderer, midX, midY, "GAME OVER", 3.0f, "TRY AGAIN? Y/N", 2.0f);
		break;
	case State::LifeLost:
		DrawMessageBox(renderer, midX, midY, "CONTINUE", 3.0f, "PRESS ANY KEY", 2.0f);
		break;
	case State::Paused:
		DrawMessageBox(renderer, midX, midY, "PAUSED", 3.0f, nullptr, 0.0f);
		break;
	case State::Playing:
		break;
	}

	// Swap front buffer and back buffer
	SDL_RenderPresent(renderer);
}
