// ----------------------------------------------------------------
// Procedural sound effects using core SDL3 audio (no SDL3_mixer, no
// asset files). Short blips are synthesized into PCM buffers up front
// and played through a small pool of audio streams so effects can
// overlap and mix.
// ----------------------------------------------------------------

#pragma once
#include <SDL3/SDL.h>

#include <vector>

class Audio
{
public:
	enum class Sound : Uint8
	{
		Paddle, // ball bounces off the paddle
		Wall,   // ball bounces off a side/top wall
		Lose,   // ball falls past the bottom
		Win,    // last brick cleared
	};

	Audio() = default;
	~Audio();
	Audio(const Audio&) = delete;
	Audio& operator=(const Audio&) = delete;

	// Open the audio device and synthesize the effect buffers (one brick blip
	// per row). Returns false if audio is unavailable -- the game stays fully
	// playable, just silent.
	bool Initialize(int brickRows);

	void Play(Sound s);
	void PlayBrick(int row); // pitch rises with the row

private:
	void PlayBuffer(const std::vector<float>& pcm);

	bool mEnabled = false;
	SDL_AudioDeviceID mDevice = 0;

	// A handful of streams bound to the device; cycling through them lets
	// near-simultaneous effects play together instead of cutting each other off.
	static const int kStreamPool = 8;
	SDL_AudioStream* mStreams[kStreamPool] = {};
	int mNextStream = 0;

	std::vector<float> mPaddle;
	std::vector<float> mWall;
	std::vector<float> mLose;
	std::vector<float> mWin;
	std::vector<std::vector<float>> mBrick; // one buffer per brick row

	// Looping background music: a pre-rendered chiptune loop fed to its own
	// stream on demand by FeedMusic (runs on the audio thread).
	void BuildMusic();
	static void SDLCALL FeedMusic(void* userdata, SDL_AudioStream* stream, int additional, int total);
	SDL_AudioStream* mMusicStream = nullptr;
	std::vector<float> mMusic;
	size_t mMusicPos = 0; // byte offset into mMusic, wrapped by FeedMusic
};
