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
#include "ui/gl/cachekey.h"
#include "ui/gl/svgdocuments.h"

#include <functional>
#include <gtest/gtest.h>
#include <string>

namespace {

constexpr const char * ICON_PATH = "res/icon/2699.svg";

} // namespace

// A loaded icon's size is cached with it, for the per-frame size queries to read
TEST(SvgIntrinsicCache, LoadedIconHasItsSizeCached)
{
    const std::string key = Ui::Gl::SvgDocuments::loadFromFile(ICON_PATH);
    ASSERT_FALSE(key.empty());

    float w = 0.0F;
    float h = 0.0F;
    EXPECT_TRUE(Ui::Gl::SvgDocuments::intrinsicSizeIfCached(key, w, h));
    EXPECT_GT(w, 0.0F);
    EXPECT_GT(h, 0.0F);
}

// Negative: an icon never loaded has no size cached, and the caller's values are left as they were
TEST(SvgIntrinsicCache, AnIconNeverLoadedHasNoSizeCached)
{
    float width  = -1.0F;
    float height = -2.0F;
    EXPECT_FALSE(Ui::Gl::SvgDocuments::intrinsicSizeIfCached("res/icon/never-loaded.svg", width, height));
    EXPECT_FLOAT_EQ(width, -1.0F);
    EXPECT_FLOAT_EQ(height, -2.0F);
}

// Loaded twice, one document: the same id and the same size, not a second parse's
TEST(SvgIntrinsicCache, LoadingAnIconAgainHandsBackTheSameOne)
{
    const std::string first = Ui::Gl::SvgDocuments::loadFromFile(ICON_PATH);
    ASSERT_FALSE(first.empty());
    float firstWidth  = 0.0F;
    float firstHeight = 0.0F;
    ASSERT_TRUE(Ui::Gl::SvgDocuments::intrinsicSizeIfCached(first, firstWidth, firstHeight));

    EXPECT_EQ(Ui::Gl::SvgDocuments::loadFromFile(ICON_PATH), first);
    EXPECT_EQ(Ui::Gl::SvgDocuments::ensureLoaded(ICON_PATH), first);
    float width  = 0.0F;
    float height = 0.0F;
    ASSERT_TRUE(Ui::Gl::SvgDocuments::intrinsicSizeIfCached(first, width, height));
    EXPECT_FLOAT_EQ(width, firstWidth);
    EXPECT_FLOAT_EQ(height, firstHeight);
}

// Negative: a file that cannot be read gives no id and leaves nothing cached under its path
TEST(SvgIntrinsicCache, AnIconThatFailsToLoadLeavesNothingCached)
{
    const std::string path = "res/icon/missing-icon.svg";
    EXPECT_TRUE(Ui::Gl::SvgDocuments::loadFromFile(path).empty());
    EXPECT_FALSE(Ui::Gl::SvgDocuments::isLoaded(path));
    float width  = -1.0F;
    float height = -1.0F;
    EXPECT_FALSE(Ui::Gl::SvgDocuments::intrinsicSizeIfCached(path, width, height));
    EXPECT_FALSE(Ui::Gl::SvgDocuments::contentSizeIfCached(path, width, height));
    EXPECT_FLOAT_EQ(width, -1.0F);
}

// Its ink without the transparent padding round it: never larger than the icon, never empty
TEST(SvgIntrinsicCache, ALoadedIconHasItsContentSizeWithinItsSize)
{
    const std::string key = Ui::Gl::SvgDocuments::loadFromFile(ICON_PATH);
    ASSERT_FALSE(key.empty());
    float width         = 0.0F;
    float height        = 0.0F;
    float contentWidth  = 0.0F;
    float contentHeight = 0.0F;
    ASSERT_TRUE(Ui::Gl::SvgDocuments::intrinsicSizeIfCached(key, width, height));
    ASSERT_TRUE(Ui::Gl::SvgDocuments::contentSizeIfCached(key, contentWidth, contentHeight));
    EXPECT_GT(contentWidth, 0.0F);
    EXPECT_GT(contentHeight, 0.0F);
    EXPECT_LE(contentWidth, width);
    EXPECT_LE(contentHeight, height);
}

// The solid variant is a document of its own under its own id, the same size as the outline
TEST(SvgIntrinsicCache, TheFilledVariantIsCachedApartAtTheSameSize)
{
    const std::string key    = Ui::Gl::SvgDocuments::loadFromFile(ICON_PATH);
    const std::string filled = Ui::Gl::SvgDocuments::loadFilledFromFile(ICON_PATH);
    ASSERT_FALSE(key.empty());
    ASSERT_FALSE(filled.empty());
    EXPECT_NE(filled, key);
    EXPECT_EQ(Ui::Gl::SvgDocuments::loadFilledFromFile(ICON_PATH), filled);

    float width        = 0.0F;
    float height       = 0.0F;
    float filledWidth  = 0.0F;
    float filledHeight = 0.0F;
    ASSERT_TRUE(Ui::Gl::SvgDocuments::intrinsicSizeIfCached(key, width, height));
    ASSERT_TRUE(Ui::Gl::SvgDocuments::intrinsicSizeIfCached(filled, filledWidth, filledHeight));
    EXPECT_FLOAT_EQ(filledWidth, width);
    EXPECT_FLOAT_EQ(filledHeight, height);
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
