// ----------------------------------------------------------------
// Stateless 2D drawing helpers built on the SDL3 renderer. These know
// nothing about the game -- they just fill shapes and text in the
// renderer's current draw colour.
// ----------------------------------------------------------------

#pragma once
#include <SDL3/SDL.h>

// Fill a circle centred on (cx, cy).
void DrawFilledCircle(SDL_Renderer* renderer, float cx, float cy, float radius);

// Fill a rectangle with rounded corners (radius is clamped to w/2 and h/2).
void DrawRoundedRect(SDL_Renderer* renderer, float x, float y, float w, float h, float radius);

// Draw debug-font text centred on (cx, cy), enlarged by 'scale'.
void DrawCenteredText(SDL_Renderer* renderer, float cx, float cy, const char* text, float scale);

// Fill a heart centred on (cx, cy) roughly 'size' tall, in the current draw colour.
void DrawHeart(SDL_Renderer* renderer, float cx, float cy, float size);

// Draw a centred navy message box with a white border, auto-sized to fit one or
// two lines of centred text. Pass nullptr for `line2` to draw a single line.
void DrawMessageBox(SDL_Renderer* renderer, float cx, float cy, const char* line1, float scale1,
                    const char* line2, float scale2);
