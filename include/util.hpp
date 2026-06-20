#pragma once

#include <algorithm>
#include <vector>
#include <ifcg/ifcg.hpp>
#include <ifcg/graphics/mesh.hpp>

#include "noise_gen.hpp"

using namespace ifcg;

struct Color {
    float r, g, b, a;

    Color operator*(float f) const {
        return {r * f, g * f, b * f, a};
    };
};

std::pair<std::vector<Vertex>, std::vector<GLuint>> createMeshDataFromNoise(
    const std::vector<float>& noise, 
    const int width, 
    const int height, 
    float intensity, 
    Color color = {1.0f, 1.0f, 1.0f, 1.0f}
);

inline std::pair<std::vector<Vertex>, std::vector<GLuint>> createMeshDataFromNoise(const std::vector<float>& noise, const int width, const int height, float intensity, Color color) {
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int currentId = y * width + x;
            float z = noise[currentId] * intensity;

            float decay = std::max(0.5f, noise[currentId]);
            Color vColor = color * decay;

            // Swap Y and Z: x, z, y
            vertices.emplace_back(x, z, y, vColor.r, vColor.g, vColor.b, color.a);
        }
    }

    for (int y = 0; y < height - 1; ++y) {
        for (int x = 0; x < width - 1; ++x) {
            int topLeft = y * width + x;
            int topRight = y * width + (x + 1);
            int bottomLeft = (y + 1) * width + x;
            int bottomRight = (y + 1) * width + (x + 1);

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    return {vertices, indices};
}