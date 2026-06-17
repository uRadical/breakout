// ----------------------------------------------------------------
// Implementations of the pure game maths (see Physics.h).
// ----------------------------------------------------------------

#include "Physics.h"

#include <algorithm> // std::clamp
#include <cmath>     // std::sqrt, std::sin, std::cos, std::fabs

Vector2 PaddleDeflection(
    Vector2 ballVel, float ballX, float paddleCentreX, float paddleWidth, float maxBounceAngle)
{
	// Offset of the hit from the paddle centre, normalised to [-1, 1].
	const float offset = std::clamp((ballX - paddleCentreX) / (paddleWidth / 2.0f), -1.0f, 1.0f);
	const float angle = offset * maxBounceAngle;
	const float speed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);
	return {speed * std::sin(angle), -speed * std::cos(angle)}; // negative y = upward
}

BounceAxis BrickBounce(Vector2 ballPos, float ballRadius, Rect brick)
{
	const float brickCx = brick.x + brick.w / 2.0f;
	const float brickCy = brick.y + brick.h / 2.0f;
	// Treat the ball as a box (centre +/- radius) and measure overlap per axis.
	const float overlapX = (brick.w / 2.0f + ballRadius) - std::fabs(ballPos.x - brickCx);
	const float overlapY = (brick.h / 2.0f + ballRadius) - std::fabs(ballPos.y - brickCy);
	if (overlapX <= 0.0f || overlapY <= 0.0f)
	{
		return BounceAxis::None;
	}
	// Reflect along whichever axis it's least overlapping (the nearest face).
	return (overlapX < overlapY) ? BounceAxis::Horizontal : BounceAxis::Vertical;
}
