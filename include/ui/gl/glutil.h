// Copyright © 2025-2026 Sergio Stopkin.

/*
 * This file is part of PureGlUi. PureGlUi is free software:
 * you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * PureGlUi is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with PureGlUi. See the file COPYING. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "ui/gl/localglew.h"
#include "ui/type.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <vector>

namespace Ui::Gl::Util {

// Number of floats per vertex in pos(x,y) + uv(u,v) layout
constexpr int POS_UV_FLOATS = 4;

// GL vertex attrib pointer offset helper (centralizes the required reinterpret_cast)
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
inline void * bufferOffset(size_t bytes) { return reinterpret_cast<void *>(bytes); }

// Compile a single shader stage; returns 0 on failure
inline GLuint compileShader(GLenum type, const char * source, const char * label)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == 0) {
        std::array<char, 1024> log {};
        glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::cerr << label << log.data() << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// Link vertex + fragment shaders into a program; deletes shaders afterward; returns 0 on failure
inline GLuint linkProgram(GLuint vertex, GLuint fragment, const char * label)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == 0) {
        std::array<char, 1024> log {};
        glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::cerr << label << log.data() << std::endl;
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}

// Top-left origin orthographic projection matrix (column-major for GL)
// Maps (0,0)-(cssW,cssH) to clip space with Y pointing down
inline std::array<fpx_t, 16> orthoProjection(fpx_t cssWidth, fpx_t cssHeight)
{
    return { 2.0F / cssWidth, 0.0F, 0.0F, 0.0F, 0.0F, -2.0F / cssHeight, 0.0F, 0.0F, 0.0F, 0.0F, -1.0F, 0.0F,
             -1.0F,           1.0F, 0.0F, 1.0F };
}

// Create VAO + VBO with vec2 position (location 0) + vec2 texcoord (location 1) attribute layout
inline void createPosUvVao(GLuint & vao, GLuint & vbo)
{
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, POS_UV_FLOATS * sizeof(float), nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, POS_UV_FLOATS * sizeof(float), bufferOffset(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Build 6-vertex textured quad (2 triangles, pos+uv interleaved)
inline std::array<float, 24> quadVertices(fpx_t x, fpx_t y, fpx_t width, fpx_t height)
{
    const fpx_t x1 = x + width;
    const fpx_t y1 = y + height;
    return {
        x,  y,  0.0F, 0.0F, x1, y,  1.0F, 0.0F, x1, y1, 1.0F, 1.0F,
        x1, y1, 1.0F, 1.0F, x,  y1, 0.0F, 1.0F, x,  y,  0.0F, 0.0F,
    };
}

// Build 6-vertex textured quad with Y-flipped UVs (for glReadPixels-sourced textures)
inline std::array<float, 24> quadVerticesFlipY(fpx_t x, fpx_t y, fpx_t width, fpx_t height)
{
    const fpx_t x1 = x + width;
    const fpx_t y1 = y + height;
    return {
        x,  y,  0.0F, 1.0F, x1, y,  1.0F, 1.0F, x1, y1, 1.0F, 0.0F,
        x1, y1, 1.0F, 0.0F, x,  y1, 0.0F, 0.0F, x,  y,  0.0F, 1.0F,
    };
}

// Iterate shadow layers from outermost to innermost, calling fn(offsetX, offsetY, expand, alpha)
template <typename Fn>
inline void forEachShadowLayer(fpx_t offsetX, fpx_t offsetY, fpx_t blurRadius, fpx_t opacity, Fn fn)
{
    constexpr int kLayers = 5;
    int           i       = kLayers;
    while (i >= 1) {
        const fpx_t spread = blurRadius * i / kLayers;
        fn(offsetX + spread * 0.3F, offsetY + spread * 0.3F, spread * 0.5F, opacity / i);
        --i;
    }
}

// Upload vertex data to the bound VBO and draw triangles
template <typename T, std::size_t N>
inline void drawTriangles(const std::array<T, N> & vertices, GLsizei vertexCount)
{
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(N * sizeof(T)), vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
}

inline void drawTriangles(const std::vector<float> & vertices)
{
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(),
                 GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / POS_UV_FLOATS));
}

// Safe GL resource deletion helpers
inline void deleteProgram(GLuint & program)
{
    if (program != 0) {
        glDeleteProgram(program);
        program = 0;
    }
}

inline void deleteBuffer(GLuint & buffer)
{
    if (buffer != 0) {
        glDeleteBuffers(1, &buffer);
        buffer = 0;
    }
}

inline void deleteVertexArray(GLuint & vao)
{
    if (vao != 0) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
}

inline void deleteTexture(GLuint & texture)
{
    if (texture != 0) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
}

} // namespace Ui::Gl::Util
