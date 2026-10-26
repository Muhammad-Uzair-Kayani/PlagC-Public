#include "pgcpch.h"
#include <PlagC/Renderer/Renderer.h>
namespace PlagC
{
MeshData MakePrimitive(Primitive type)
{
    MeshData m;
    auto quad = [&](Vec3 n, Vec3 u, Vec3 v, Vec3 c) {
        auto b = static_cast<std::uint32_t>(m.vertices.size());
        for (auto uv : {std::pair{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}})
        {
            float a = uv.first - .5f, d = uv.second - .5f;
            m.vertices.push_back(
                {{c.x + a * u.x + d * v.x, c.y + a * u.y + d * v.y, c.z + a * u.z + d * v.z},
                 n,
                 uv.first,
                 uv.second,
                 u});
        }
        m.indices.insert(m.indices.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
    };
    constexpr float pi = 3.14159265358979323846f;
    if (type == Primitive::Cube)
    {
        quad({1, 0, 0}, {0, 0, -1}, {0, 1, 0}, {.5f, 0, 0});
        quad({-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, {-.5f, 0, 0});
        quad({0, 1, 0}, {1, 0, 0}, {0, 0, -1}, {0, .5f, 0});
        quad({0, -1, 0}, {1, 0, 0}, {0, 0, 1}, {0, -.5f, 0});
        quad({0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {0, 0, .5f});
        quad({0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, {0, 0, -.5f});
    }
    else if (type == Primitive::Plane)
        quad({0, 1, 0}, {1, 0, 0}, {0, 0, -1}, {});
    else if (type == Primitive::Sphere)
    {
        constexpr unsigned rings = 16, slices = 32;
        for (unsigned y = 0; y <= rings; ++y)
            for (unsigned x = 0; x <= slices; ++x)
            {
                float v = float(y) / rings, u = float(x) / slices, p = v * pi, t = u * 2 * pi;
                Vec3 n{std::sin(p) * std::cos(t), std::cos(p), std::sin(p) * std::sin(t)};
                m.vertices.push_back({{n.x * .5f, n.y * .5f, n.z * .5f}, n, u, v});
            }
        for (unsigned y = 0; y < rings; ++y)
            for (unsigned x = 0; x < slices; ++x)
            {
                unsigned a = y * (slices + 1) + x, b = a + slices + 1;
                if (y)
                    m.indices.insert(m.indices.end(), {a, a + 1, b});
                if (y + 1 < rings)
                    m.indices.insert(m.indices.end(), {a + 1, b + 1, b});
            }
    }
    else if (type == Primitive::Cylinder)
    {
        constexpr unsigned slices = 32;
        for (unsigned x = 0; x <= slices; ++x)
        {
            float u = float(x) / slices, t = u * 2 * pi;
            Vec3 n{std::cos(t), 0, std::sin(t)};
            for (float y : {-.5f, .5f})
                m.vertices.push_back({{n.x * .5f, y, n.z * .5f}, n, u, y + .5f});
        }
        for (unsigned x = 0; x < slices; ++x)
        {
            unsigned a = x * 2;
            m.indices.insert(m.indices.end(), {a, a + 1, a + 2, a + 1, a + 3, a + 2});
        }
        for (float sign : {-1.f, 1.f})
        {
            auto b = static_cast<unsigned>(m.vertices.size());
            m.vertices.push_back({{0, sign * .5f, 0}, {0, sign, 0}, .5f, .5f});
            for (unsigned x = 0; x <= slices; ++x)
            {
                float t = float(x) / slices * 2 * pi, c = std::cos(t) * .5f, s = std::sin(t) * .5f;
                m.vertices.push_back({{c, sign * .5f, s}, {0, sign, 0}, c + .5f, s + .5f});
            }
            for (unsigned x = 0; x < slices; ++x)
            {
                if (sign > 0)
                    m.indices.insert(m.indices.end(), {b, b + x + 2, b + x + 1});
                else
                    m.indices.insert(m.indices.end(), {b, b + x + 1, b + x + 2});
            }
        }
    }
    else
        throw std::invalid_argument("unknown primitive");
    return m;
}
} // namespace PlagC
