// Unit tests for the pure game maths in Physics.h. No SDL, no framework -- a
// tiny CHECK macro keeps the test build dependency-free; run via `ctest`.

#include "Physics.h"

#include <cmath>
#include <cstdio>
#include <numbers>

namespace
{
int g_failures = 0;

#define CHECK(cond)                                                     \
	do                                                                  \
	{                                                                   \
		if (!(cond))                                                    \
		{                                                               \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures;                                               \
		}                                                               \
	} while (0)

bool Approx(float a, float b, float eps = 1e-3f)
{
	return std::fabs(a - b) < eps;
}

float Speed(Vector2 v)
{
	return std::sqrt(v.x * v.x + v.y * v.y);
}

constexpr float kMaxAngle = std::numbers::pi_v<float> / 3.0f; // 60 degrees

void TestPaddleDeflection()
{
	// A centre hit goes straight up, with speed preserved.
	{
		const Vector2 v = PaddleDeflection({0.0f, 300.0f}, 100.0f, 100.0f, 100.0f, kMaxAngle);
		CHECK(Approx(v.x, 0.0f));
		CHECK(v.y < 0.0f); // upward
		CHECK(Approx(Speed(v), 300.0f));
	}
	// Hitting the right half sends the ball right; the left half sends it left.
	{
		const Vector2 right = PaddleDeflection({-200.0f, 235.0f}, 140.0f, 100.0f, 100.0f, kMaxAngle);
		CHECK(right.x > 0.0f);
		CHECK(right.y < 0.0f);
		const Vector2 left = PaddleDeflection({0.0f, 235.0f}, 60.0f, 100.0f, 100.0f, kMaxAngle);
		CHECK(left.x < 0.0f);
	}
	// A hit past the edge is clamped to the max angle: speed preserved and the
	// vertical component is speed*cos(60deg) = speed/2.
	{
		const float speed = 300.0f;
		const Vector2 v = PaddleDeflection({0.0f, speed}, 500.0f, 100.0f, 100.0f, kMaxAngle);
		CHECK(Approx(Speed(v), speed));
		CHECK(Approx(v.y, -speed * 0.5f, 0.05f));
	}
}

void TestBrickBounce()
{
	const Rect brick{100.0f, 100.0f, 60.0f, 20.0f}; // centre (130, 110)

	// Far away: no collision.
	CHECK(BrickBounce({0.0f, 0.0f}, 5.0f, brick) == BounceAxis::None);
	// Approaching the flat face from below: vertical (least penetration on y).
	CHECK(BrickBounce({130.0f, 118.0f}, 5.0f, brick) == BounceAxis::Vertical);
	// Approaching from the side: horizontal.
	CHECK(BrickBounce({97.0f, 110.0f}, 5.0f, brick) == BounceAxis::Horizontal);
	// Just touching the edge counts as no overlap (boundary case).
	CHECK(BrickBounce({35.0f, 110.0f}, 5.0f, brick) == BounceAxis::None);
}
} // namespace

int main()
{
	TestPaddleDeflection();
	TestBrickBounce();

	if (g_failures == 0)
	{
		std::printf("All physics tests passed.\n");
		return 0;
	}
	std::printf("%d physics test(s) failed.\n", g_failures);
	return 1;
}
