// ----------------------------------------------------------------
// Procedural sound effects (see Audio.h). Effects are tiny synthesized
// waveforms with a short attack/decay envelope so they don't click.
// ----------------------------------------------------------------

#include "Audio.h"

#include <numbers> // std::numbers::pi_v

namespace
{
const int kRate = 44100; // samples per second (mono, 32-bit float)
const float kTwoPi = 2.0f * std::numbers::pi_v<float>;

// Apply a short linear attack and a decay-to-zero so a clipped sine/square
// doesn't pop at the start or end.
float Envelope(float t, float duration)
{
	const float attack = 0.005f; // 5 ms fade-in
	if (t < attack)
	{
		return t / attack;
	}
	const float decay = 1.0f - (t - attack) / (duration - attack);
	return decay > 0.0f ? decay : 0.0f;
}

// A fixed-frequency tone. Square waves give the bright, retro arcade blip;
// sine is mellower (used for the win chord).
std::vector<float> MakeTone(float freq, float duration, float volume, bool square)
{
	const int n = static_cast<int>(duration * kRate);
	std::vector<float> buf(static_cast<size_t>(n));
	for (int i = 0; i < n; ++i)
	{
		const float t = static_cast<float>(i) / kRate;
		const float phase = t * freq;
		const float frac = phase - SDL_floorf(phase);
		const float sample = square ? (frac < 0.5f ? 1.0f : -1.0f) : SDL_sinf(kTwoPi * phase);
		buf[static_cast<size_t>(i)] = sample * volume * Envelope(t, duration);
	}
	return buf;
}

// A tone whose pitch glides from f0 to f1 (used for the descending "lose" sound).
std::vector<float> MakeSweep(float f0, float f1, float duration, float volume)
{
	const int n = static_cast<int>(duration * kRate);
	std::vector<float> buf(static_cast<size_t>(n));
	float phase = 0.0f;
	for (int i = 0; i < n; ++i)
	{
		const float k = static_cast<float>(i) / static_cast<float>(n);
		const float freq = f0 + (f1 - f0) * k;
		phase += freq / kRate;
		const float t = static_cast<float>(i) / kRate;
		buf[static_cast<size_t>(i)] = SDL_sinf(kTwoPi * phase) * volume * Envelope(t, duration);
	}
	return buf;
}

void Append(std::vector<float>& dst, const std::vector<float>& src)
{
	dst.insert(dst.end(), src.begin(), src.end());
}

// Add `src` onto `dst` starting at sample offset `at`, growing dst if needed.
void MixInto(std::vector<float>& dst, const std::vector<float>& src, size_t at)
{
	if (at + src.size() > dst.size())
	{
		dst.resize(at + src.size(), 0.0f);
	}
	for (size_t i = 0; i < src.size(); ++i)
	{
		dst[at + i] += src[i];
	}
}

// Equal-tempered pitch of a MIDI note number (69 = A4 = 440 Hz).
float MidiToFreq(int note)
{
	return 440.0f * SDL_powf(2.0f, static_cast<float>(note - 69) / 12.0f);
}
} // namespace

bool Audio::Initialize(int brickRows)
{
	if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
	{
		SDL_Log("Audio unavailable (continuing silent): %s", SDL_GetError());
		return false;
	}

	SDL_AudioSpec spec;
	spec.format = SDL_AUDIO_F32;
	spec.channels = 1;
	spec.freq = kRate;

	mDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
	if (mDevice == 0)
	{
		SDL_Log("Could not open audio device (continuing silent): %s", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return false;
	}

	for (int i = 0; i < kStreamPool; ++i)
	{
		mStreams[i] = SDL_CreateAudioStream(&spec, &spec);
		if (mStreams[i] == nullptr)
		{
			SDL_Log("Could not create audio stream (continuing silent): %s", SDL_GetError());
			return false; // destructor cleans up whatever was created
		}
		SDL_BindAudioStream(mDevice, mStreams[i]);
	}
	SDL_ResumeAudioDevice(mDevice);

	// Synthesize the effect buffers once.
	mPaddle = MakeTone(440.0f, 0.06f, 0.35f, true);
	mWall = MakeTone(300.0f, 0.05f, 0.30f, true);
	mLose = MakeSweep(420.0f, 110.0f, 0.45f, 0.40f);

	// Win: a quick C-E-G major arpeggio (sine, gentler than the blips).
	Append(mWin, MakeTone(523.25f, 0.10f, 0.35f, false)); // C5
	Append(mWin, MakeTone(659.25f, 0.10f, 0.35f, false)); // E5
	Append(mWin, MakeTone(783.99f, 0.16f, 0.35f, false)); // G5

	// One brick blip per row; pitch rises toward the top rows.
	mBrick.resize(static_cast<size_t>(brickRows));
	for (int row = 0; row < brickRows; ++row)
	{
		const float freq = 400.0f + static_cast<float>(brickRows - 1 - row) * 45.0f;
		mBrick[static_cast<size_t>(row)] = MakeTone(freq, 0.05f, 0.30f, true);
	}

	// Render the looping background music and feed it on its own stream. Mixed
	// quietly under the effects via a lower stream gain.
	BuildMusic();
	if (!mMusic.empty())
	{
		mMusicStream = SDL_CreateAudioStream(&spec, &spec);
		if (mMusicStream != nullptr)
		{
			SDL_SetAudioStreamGetCallback(mMusicStream, FeedMusic, this);
			SDL_SetAudioStreamGain(mMusicStream, 0.55f);
			SDL_BindAudioStream(mDevice, mMusicStream);
		}
	}

	mEnabled = true;
	return true;
}

Audio::~Audio()
{
	if (mMusicStream != nullptr)
	{
		SDL_DestroyAudioStream(mMusicStream); // stops the feed callback
	}
	for (SDL_AudioStream* stream : mStreams)
	{
		if (stream != nullptr)
		{
			SDL_DestroyAudioStream(stream);
		}
	}
	if (mDevice != 0)
	{
		SDL_CloseAudioDevice(mDevice);
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}
}

void Audio::PlayBuffer(const std::vector<float>& pcm)
{
	if (!mEnabled || pcm.empty())
	{
		return;
	}
	// Round-robin a stream so overlapping effects mix instead of cutting off.
	SDL_AudioStream* stream = mStreams[mNextStream];
	mNextStream = (mNextStream + 1) % kStreamPool;
	SDL_ClearAudioStream(stream);
	SDL_PutAudioStreamData(stream, pcm.data(), static_cast<int>(pcm.size() * sizeof(float)));
}

void Audio::Play(Sound s)
{
	switch (s)
	{
	case Sound::Paddle:
		PlayBuffer(mPaddle);
		break;
	case Sound::Wall:
		PlayBuffer(mWall);
		break;
	case Sound::Lose:
		PlayBuffer(mLose);
		break;
	case Sound::Win:
		PlayBuffer(mWin);
		break;
	}
}

void Audio::PlayBrick(int row)
{
	if (row >= 0 && row < static_cast<int>(mBrick.size()))
	{
		PlayBuffer(mBrick[static_cast<size_t>(row)]);
	}
}

void Audio::BuildMusic()
{
	// An upbeat 4-bar chiptune loop over a vi-IV-I-V progression (Am-F-C-G):
	// eight eighth-note lead steps per bar with a driving square bass.
	const float bpm = 132.0f;
	const float eighth = 60.0f / bpm / 2.0f; // seconds per eighth note

	// Lead melody as MIDI note numbers (0 = rest), 4 bars x 8 steps.
	static const int lead[4][8] = {
	    {69, 72, 76, 72, 69, 72, 76, 74}, // Am
	    {72, 69, 65, 69, 72, 69, 67, 0},  // F
	    {76, 79, 76, 72, 67, 72, 76, 79}, // C
	    {74, 71, 67, 71, 74, 79, 74, 0},  // G
	};
	// One bass note per bar (low octave), re-attacked each eighth for drive.
	static const int bass[4] = {45, 41, 48, 43}; // A2, F2, C3, G2

	for (int bar = 0; bar < 4; ++bar)
	{
		for (int step = 0; step < 8; ++step)
		{
			const size_t at = mMusic.size();
			if (lead[bar][step] > 0) // lead voice (skip rests)
			{
				MixInto(mMusic, MakeTone(MidiToFreq(lead[bar][step]), eighth, 0.16f, true), at);
			}
			MixInto(mMusic, MakeTone(MidiToFreq(bass[bar]), eighth, 0.12f, true), at); // bass voice
		}
	}
}

// Audio-thread callback: keep the music stream fed by copying from the loop
// buffer, wrapping back to the start so it plays seamlessly forever.
void SDLCALL Audio::FeedMusic(void* userdata, SDL_AudioStream* stream, int additional, int /*total*/)
{
	Audio* self = static_cast<Audio*>(userdata);
	const int bytes = static_cast<int>(self->mMusic.size() * sizeof(float));
	if (bytes <= 0)
	{
		return;
	}
	const Uint8* base = reinterpret_cast<const Uint8*>(self->mMusic.data());
	while (additional > 0)
	{
		const int pos = static_cast<int>(self->mMusicPos);
		int chunk = bytes - pos;
		if (chunk > additional)
		{
			chunk = additional;
		}
		SDL_PutAudioStreamData(stream, base + pos, chunk);
		self->mMusicPos = static_cast<size_t>((pos + chunk) % bytes);
		additional -= chunk;
	}
}
