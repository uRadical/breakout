// ----------------------------------------------------------------
// Pure, SDL-free game maths. Keeping the collision and deflection logic here
// (free functions over plain structs) lets it be unit-tested in isolation.
// ----------------------------------------------------------------

#pragma once

// 2D vector of x/y coordinates (also used for positions and velocities).
struct Vector2
{
	float x = 0.0f;
	float y = 0.0f;

	Vector2 operator+(Vector2 o) const { return {x + o.x, y + o.y}; }
	Vector2 operator-(Vector2 o) const { return {x - o.x, y - o.y}; }
	Vector2 operator*(float s) const { return {x * s, y * s}; }
	Vector2& operator+=(Vector2 o)
	{
		x += o.x;
		y += o.y;
		return *this;
	}
};

// Axis-aligned rectangle (top-left x/y plus width/height).
struct Rect
{
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;
};

// Which velocity component a bounce should flip (or neither).
enum class BounceAxis : unsigned char
{
	None,
	Horizontal, // flip x velocity
	Vertical,   // flip y velocity
};

// New ball velocity after striking the paddle. The rebound angle is derived from
// where the ball hit relative to the paddle centre (centre -> straight up, edges
// -> +/- maxBounceAngle from vertical, clamped); the ball's speed is preserved.
Vector2 PaddleDeflection(
    Vector2 ballVel, float ballX, float paddleCentreX, float paddleWidth, float maxBounceAngle);

// For a ball (circle) overlapping a brick, the axis of least penetration to
// reflect on; BounceAxis::None if they don't overlap.
BounceAxis BrickBounce(Vector2 ballPos, float ballRadius, Rect brick);
