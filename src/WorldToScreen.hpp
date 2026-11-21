#pragma once
#include <cstdint>
#include <optional>

namespace wrench {

    struct Vec3 { float x, y, z; };
    struct Vec2 { float x, y; };

    // Row-major 4x4, Source2 convention: row 0-3 are the matrix rows, element [row][col].
    struct Mat4 { float m[4][4]; };

    // Pure, checkable without the game: projects a world point to screen pixels.
    // Returns nullopt if the point is behind the camera (w <= 0) � never silently clamps it.
    inline std::optional<Vec2> WorldToScreen(const Vec3& world, const Mat4& viewProj,
        int screenWidth, int screenHeight) noexcept {
        const float x = world.x * viewProj.m[0][0] + world.y * viewProj.m[0][1] +
            world.z * viewProj.m[0][2] + viewProj.m[0][3];
        const float y = world.x * viewProj.m[1][0] + world.y * viewProj.m[1][1] +
            world.z * viewProj.m[1][2] + viewProj.m[1][3];
        const float w = world.x * viewProj.m[3][0] + world.y * viewProj.m[3][1] +
            world.z * viewProj.m[3][2] + viewProj.m[3][3];

        if (w <= 0.0001f) return std::nullopt;  // behind camera or degenerate, don't fake a point

        const float ndcX = x / w;
        const float ndcY = y / w;

        Vec2 screen;
        screen.x = (screenWidth / 2.0f) * (1.0f + ndcX);
        screen.y = (screenHeight / 2.0f) * (1.0f - ndcY);
        return screen;
    }

}  // namespace wrench
