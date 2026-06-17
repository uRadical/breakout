// ----------------------------------------------------------------
// Implementations of the stateless drawing helpers (see Draw.h).
// ----------------------------------------------------------------

#include "Draw.h"

// SDL's 2D renderer has no circle primitive, so fill one by stacking
// horizontal scanlines: for each row, draw a line as wide as the circle there.
void DrawFilledCircle(SDL_Renderer* renderer, float cx, float cy, float radius)
{
	const int r = static_cast<int>(radius);
	for (int dy = -r; dy <= r; ++dy)
	{
		const float fdy = static_cast<float>(dy);
		const float dx = SDL_sqrtf(radius * radius - fdy * fdy);
		SDL_RenderLine(renderer, cx - dx, cy + fdy, cx + dx, cy + fdy);
	}
}

// Draw a filled rectangle with rounded corners. Filled as contiguous 1px rows
// (each row inset near the corners by the circle profile) so there are no seams
// between bands and corners -- the previous band+circle approach left a thin
// sub-pixel gap along the bottom/top edges.
void DrawRoundedRect(SDL_Renderer* renderer, float x, float y, float w, float h, float radius)
{
	if (radius > w / 2.0f)
	{
		radius = w / 2.0f;
	}
	if (radius > h / 2.0f)
	{
		radius = h / 2.0f;
	}

	const int yTop = static_cast<int>(SDL_floorf(y));
	const int yBottom = static_cast<int>(SDL_ceilf(y + h));
	for (int py = yTop; py < yBottom; ++py)
	{
		const float rowCentre = static_cast<float>(py) + 0.5f;
		float inset = 0.0f;
		if (rowCentre < y + radius) // within the top corners
		{
			const float d = (y + radius) - rowCentre;
			inset = radius - SDL_sqrtf(radius * radius - d * d);
		}
		else if (rowCentre > y + h - radius) // within the bottom corners
		{
			const float d = rowCentre - (y + h - radius);
			inset = radius - SDL_sqrtf(radius * radius - d * d);
		}
		SDL_FRect rowRect{x + inset, static_cast<float>(py), w - 2.0f * inset, 1.0f};
		SDL_RenderFillRect(renderer, &rowRect);
	}
}

// Draw debug-font text centred on (cx, cy), enlarged by 'scale'. Uses the
// current draw color. SDL3's built-in font is 8x8 px per character.
void DrawCenteredText(SDL_Renderer* renderer, float cx, float cy, const char* text, float scale)
{
	const float charSize = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
	const float len = static_cast<float>(SDL_strlen(text));
	// SDL_SetRenderScale multiplies all coordinates, so work in scaled space.
	SDL_SetRenderScale(renderer, scale, scale);
	const float x = cx / scale - (len * charSize) / 2.0f;
	const float y = cy / scale - charSize / 2.0f;
	SDL_RenderDebugText(renderer, x, y, text);
	SDL_SetRenderScale(renderer, 1.0f, 1.0f);
}

// A heart is two overlapping circles for the top lobes plus a triangle tapering
// to the point. The circles use the current draw colour directly; the triangle
// is drawn via SDL_RenderGeometry, which needs the colour on each vertex.
void DrawHeart(SDL_Renderer* renderer, float cx, float cy, float size)
{
	const float lobeRadius = size * 0.27f;
	const float lobeOffsetX = size * 0.25f;
	const float lobeY = cy - size * 0.18f;
	const float pointY = cy + size * 0.42f;

	DrawFilledCircle(renderer, cx - lobeOffsetX, lobeY, lobeRadius);
	DrawFilledCircle(renderer, cx + lobeOffsetX, lobeY, lobeRadius);

	Uint8 r = 0, g = 0, b = 0, a = 0;
	SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
	const SDL_FColor colour{static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
	                        static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f};
	const float halfWidth = lobeOffsetX + lobeRadius; // widest extent of the lobes
	const SDL_Vertex verts[3] = {
	    {{cx - halfWidth, lobeY}, colour, {0.0f, 0.0f}},
	    {{cx + halfWidth, lobeY}, colour, {0.0f, 0.0f}},
	    {{cx, pointY}, colour, {0.0f, 0.0f}},
	};
	SDL_RenderGeometry(renderer, nullptr, verts, 3, nullptr, 0);
}

// Draw a centred navy message box (white border) sized to fit one or two lines
// of centred text. Pass nullptr for `line2` to draw a single line.
void DrawMessageBox(SDL_Renderer* renderer, float cx, float cy, const char* line1, float scale1,
                    const char* line2, float scale2)
{
	const float charSize = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
	const float padX = 60.0f;
	const float padY = 34.0f;
	const float lineGap = 16.0f;

	const float w1 = static_cast<float>(SDL_strlen(line1)) * charSize * scale1;
	const float h1 = charSize * scale1;
	const bool twoLines = (line2 != nullptr);
	const float w2 = twoLines ? static_cast<float>(SDL_strlen(line2)) * charSize * scale2 : 0.0f;
	const float h2 = twoLines ? charSize * scale2 : 0.0f;

	const float contentW = (w1 > w2) ? w1 : w2;
	const float textH = twoLines ? (h1 + lineGap + h2) : h1;
	const SDL_FRect box{cx - (contentW + 2.0f * padX) / 2.0f, cy - (textH + 2.0f * padY) / 2.0f,
	                    contentW + 2.0f * padX, textH + 2.0f * padY};

	SDL_SetRenderDrawColor(renderer, 12, 26, 80, 255); // navy panel
	SDL_RenderFillRect(renderer, &box);
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // white border + text
	SDL_RenderRect(renderer, &box);

	if (twoLines)
	{
		const float top = cy - textH / 2.0f;
		DrawCenteredText(renderer, cx, top + h1 / 2.0f, line1, scale1);
		DrawCenteredText(renderer, cx, top + h1 + lineGap + h2 / 2.0f, line2, scale2);
	}
	else
	{
		DrawCenteredText(renderer, cx, cy, line1, scale1);
	}
}
