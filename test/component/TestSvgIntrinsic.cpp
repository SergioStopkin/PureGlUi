// Copyright © 2025-2026 Sergio Stopkin.

/*
 * This file is part of PureCreator. PureCreator is free software:
 * you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * PureCreator is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with PureCreator. See the file COPYING. If not, see <https://www.gnu.org/licenses/>.
 */

#include "common/hash.h"
#include "ui/gl/svgrenderer.h"

#include <functional>
#include <gtest/gtest.h>

using namespace Ui::Gl;

TEST(SvgIntrinsicCache, PreloadedIconsDoNotInstrumentIntrinsicQueries)
{
    // Ensure a clean state
    SvgRenderer::resetInstrumentationCounters();

    // Preload an icon into process-wide cache
    SvgRenderer loader;
    std::string key = loader.loadFromFile("res/icon/2699.svg");
    ASSERT_FALSE(key.empty());

    // Fast-path lookup should succeed and must not increment intrinsic-size counter
    float w = 0.0F, h = 0.0F;
    bool  cached = loader.intrinsicSizeIfCached(key, w, h);
    EXPECT_TRUE(cached);
    EXPECT_EQ(SvgRenderer::intrinsicSizeCalled(), 0U);
}

TEST(SvgCacheKey, SizesHashApart)
{
    // A hand fold of hash(id) ^ (width << 1) ^ (height << 2) hashes 32x16 as 0x0
    const Ui::Gl::cache_key_t wide { "a", 32, 16 };
    const Ui::Gl::cache_key_t tall { "a", 16, 32 };
    const Ui::Gl::cache_key_t empty { "a", 0, 0 };
    EXPECT_EQ(std::hash<Ui::Gl::cache_key_t> {}(wide), Common::hashOf(wide.svgId, wide.width, wide.height));
    EXPECT_NE(std::hash<Ui::Gl::cache_key_t> {}(wide), std::hash<Ui::Gl::cache_key_t> {}(tall));
    EXPECT_NE(std::hash<Ui::Gl::cache_key_t> {}(wide), std::hash<Ui::Gl::cache_key_t> {}(empty));
    EXPECT_NE(std::hash<Ui::Gl::cache_key_t> {}(tall), std::hash<Ui::Gl::cache_key_t> {}(empty));
}
