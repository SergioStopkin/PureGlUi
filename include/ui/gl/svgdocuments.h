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

#include "common/bytes.h"
#include "ui/gl/contentbounds.h"
#include "ui/gl/contentboundsof.h"
#include "ui/gl/intrinsicsizeof.h"
#include "ui/gl/rgbaof.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <future>
#include <glib-object.h>
#include <glib.h>
#include <iostream>
#include <iterator>
#include <librsvg/rsvg.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Ui::Gl {

// Flip to true to report each SVG the first time it is rasterized, with the
// size and content bounds it resolved to. Useful when an icon renders at the
// wrong scale or comes out blank.
constexpr bool SVG_DEBUG = false;

// The SVG documents the process has parsed, by id (a file's path, or "filled:" before it for
// the solid variant), each with its intrinsic size and content bounds: shared by every
// window's SvgRenderer, so a document is parsed once whichever renderer asks first, and
// safe to ask from any thread
class SvgDocuments final {
public:
    SvgDocuments() = delete;

    /**
     * @brief Load SVG from file and cache the document
     * @param path File path to SVG
     * @return Document ID (the path) or empty string on failure
     */
    static std::string loadFromFile(const std::string & path)
    {
        // Fast check if already present
        {
            const std::scoped_lock lock(s_documentsMutex);
            if (s_documents.contains(path)) {
                return path;
            }
        }

        // If another thread is loading this path, wait for its future
        {
            const std::scoped_lock lock(s_loadsMutex);
            auto                   it = s_loads.find(path);
            if (it != s_loads.end()) {
                auto spdoc = it->second.get();
                if (spdoc != nullptr) {
                    return path;
                }
                // previous load failed; fall through and try again
            }
        }

        // Mark that we are loading this path by creating a promise
        std::promise<document_ptr_t> prom;
        auto                         fut = prom.get_future().share();
        {
            const std::scoped_lock lock(s_loadsMutex);
            s_loads[path] = fut;
        }

        GError *     gerr  = nullptr;
        GFile *      gfile = g_file_new_for_path(path.c_str());
        RsvgHandle * raw   = rsvg_handle_new_from_gfile_sync(gfile, RSVG_HANDLE_FLAGS_NONE, nullptr, &gerr);
        g_object_unref(gfile);

        if (raw == nullptr) {
            std::cerr << "[SvgRenderer] Failed to load SVG: " << path;
            if (gerr != nullptr) {
                std::cerr << " (" << gerr->message << ")";
                g_error_free(gerr);
            }
            std::cerr << std::endl;
            // Notify waiters of failure
            prom.set_value(nullptr);
            {
                const std::scoped_lock lock(s_loadsMutex);
                s_loads.erase(path);
            }
            return "";
        }

        auto sdoc = wrapHandle(raw);

        // Query intrinsic size
        const auto [w, h] = Ui::Gl::intrinsicSizeOf(raw);

        // Compute tight content bounding box (excludes transparent padding)
        const content_bounds_t cb = Ui::Gl::contentBoundsOf(raw, w, h);

        {
            const std::scoped_lock lock(s_documentsMutex);
            s_documents[path]     = sdoc;
            s_sizes[path]         = { w, h };
            s_contentBounds[path] = cb;
        }

        // Notify waiters with the loaded document
        prom.set_value(sdoc);

        if constexpr (SVG_DEBUG) {
            if (s_loggedIds.insert(path).second) {
                std::cout << "[SvgRenderer] Loaded SVG: " << path << " -> " << w << "x" << h << " content=[" << cb.u0
                          << "," << cb.v0 << "," << cb.u1 << "," << cb.v1 << "]" << std::endl;
            }
        }

        // Clean up future entry
        {
            const std::scoped_lock lock(s_loadsMutex);
            s_loads.erase(path);
        }

        return path;
    }

    /**
     * @brief Ensure the SVG is loaded, the cache asked first
     * @param path File path to SVG
     * @return Document ID or empty string on failure
     */
    static std::string ensureLoaded(const std::string & path)
    {
        // Fast path: already loaded in shared cache
        {
            const std::scoped_lock lock(s_documentsMutex);
            if (s_documents.contains(path)) {
                return path;
            }
        }

        return loadFromFile(path);
    }

    /**
     * @brief Load SVG from file with fill="none" replaced by fill="#000000"
     * Used for outline-only SVGs that need to be tinted as solid shapes at runtime.
     * @return Document ID (prefixed with "filled:") or empty string on failure
     */
    static std::string loadFilledFromFile(const std::string & path)
    {
        std::string filledId = "filled:" + path;

        // Check if already loaded
        {
            const std::scoped_lock lock(s_documentsMutex);
            if (s_documents.contains(filledId)) {
                return filledId;
            }
        }

        // Read file content
        std::ifstream file(path);
        if (!file.is_open()) {
            return {};
        }
        std::string svgData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        // Replace fill="none" with fill="#000000"
        const std::string_view target      = "fill=\"none\"";
        const std::string_view replacement = "fill=\"#000000\"";
        std::size_t            pos         = 0;
        while ((pos = svgData.find(target, pos)) != std::string::npos) {
            svgData.replace(pos, target.size(), replacement);
            pos += replacement.size();
        }

        return loadFromString(svgData, filledId);
    }

    /**
     * @brief Check if SVG is loaded
     */
    static bool isLoaded(const std::string & svgId)
    {
        const std::scoped_lock lock(s_documentsMutex);
        return s_documents.contains(svgId);
    }

    // Fast path: return intrinsic size only if already cached in process-wide size cache
    static bool intrinsicSizeIfCached(const std::string & svgId, float & outW, float & outH)
    {
        const std::scoped_lock lock(s_documentsMutex);
        auto                   cacheIt = s_sizes.find(svgId);
        if (cacheIt != s_sizes.end()) {
            outW = cacheIt->second.first;
            outH = cacheIt->second.second;
            return true;
        }
        return false;
    }

    // Fast path: return content size (intrinsic size minus padding) only if already cached
    // Returns the tight content dimensions, excluding transparent alpha padding
    static bool contentSizeIfCached(const std::string & svgId, float & outW, float & outH)
    {
        const std::scoped_lock lock(s_documentsMutex);

        auto sizeIt = s_sizes.find(svgId);
        if (sizeIt == s_sizes.end()) {
            return false;
        }
        const float intrW = sizeIt->second.first;
        const float intrH = sizeIt->second.second;

        auto cbIt = s_contentBounds.find(svgId);
        if (cbIt != s_contentBounds.end()) {
            outW = intrW * (cbIt->second.u1 - cbIt->second.u0);
            outH = intrH * (cbIt->second.v1 - cbIt->second.v0);
        } else {
            outW = intrW;
            outH = intrH;
        }
        return true;
    }

    // A loaded SVG rendered to `width` x `height` RGBA pixels, premultiplied or not; empty when
    // it is not loaded or fails to render. Content bounds that differ meaningfully from the
    // full viewBox crop its transparent padding away, the ink centred in the target
    static std::vector<uint8_t> rgbaOf(const std::string & svgId, int width, int height, bool isPremultiplied)
    {
        document_ptr_t                  doc;
        float                           svgW = 0;
        float                           svgH = 0;
        std::optional<content_bounds_t> crop;
        {
            const std::scoped_lock lock(s_documentsMutex);
            auto                   it = s_documents.find(svgId);
            if (it == s_documents.end()) {
                return {};
            }
            doc = it->second;

            auto sIt = s_sizes.find(svgId);
            if (sIt != s_sizes.end()) {
                svgW = sIt->second.first;
                svgH = sIt->second.second;
            }

            auto cbIt = s_contentBounds.find(svgId);
            if (cbIt != s_contentBounds.end()) {
                const content_bounds_t & cb  = cbIt->second;
                const float              cbW = cb.u1 - cb.u0;
                const float              cbH = cb.v1 - cb.v0;
                if (cbW > 0.01F && cbH > 0.01F && (cbW < 0.99F || cbH < 0.99F)) {
                    crop = cb;
                }
            }
        }
        return Ui::Gl::rgbaOf(doc.get(), width, height, crop, svgW, svgH, isPremultiplied);
    }

private:
    using document_ptr_t = std::shared_ptr<RsvgHandle>;

    inline static std::mutex s_documentsMutex; // guards s_documents, s_sizes, s_contentBounds
    inline static std::mutex s_loadsMutex;     // guards s_loads

    inline static std::unordered_map<std::string, document_ptr_t> s_documents; // parsed once, whichever renderer asks
    inline static std::unordered_map<std::string, std::pair<float, float>> s_sizes;         // intrinsic px
    inline static std::unordered_map<std::string, content_bounds_t>        s_contentBounds; // the ink, padding left out
    inline static std::unordered_map<std::string, std::shared_future<document_ptr_t>> s_loads; // in flight, none twice
    inline static std::unordered_set<std::string> s_loggedIds; // SVG_DEBUG's, each id once across windows

    static document_ptr_t wrapHandle(RsvgHandle * raw)
    {
        return { raw, [](RsvgHandle * h) {
                    if (h != nullptr) {
                        g_object_unref(h);
                    }
                } };
    }

    /**
     * @brief Load SVG from string data
     * @param svgData SVG XML string
     * @param id Unique identifier for caching
     * @return Document ID or empty string on failure
     */
    static std::string loadFromString(const std::string & svgData, const std::string & id)
    {
        // Fast check
        {
            const std::scoped_lock lock(s_documentsMutex);
            if (s_documents.contains(id)) {
                return id;
            }
        }

        // Wait for existing load if present
        {
            const std::scoped_lock lock(s_loadsMutex);
            auto                   it = s_loads.find(id);
            if (it != s_loads.end()) {
                auto spdoc = it->second.get();
                if (spdoc != nullptr) {
                    return id;
                }
            }
        }

        std::promise<document_ptr_t> prom;
        auto                         fut = prom.get_future().share();
        {
            const std::scoped_lock lock(s_loadsMutex);
            s_loads[id] = fut;
        }

        GError *     gerr = nullptr;
        RsvgHandle * raw  = rsvg_handle_new_from_data(Common::asBytes(svgData.data()), svgData.size(), &gerr);

        if (raw == nullptr) {
            std::cerr << "[SvgRenderer] Failed to parse SVG string: " << id;
            if (gerr != nullptr) {
                std::cerr << " (" << gerr->message << ")";
                g_error_free(gerr);
            }
            std::cerr << std::endl;
            prom.set_value(nullptr);
            {
                const std::scoped_lock lock(s_loadsMutex);
                s_loads.erase(id);
            }
            return "";
        }

        auto sdoc = wrapHandle(raw);

        const auto [w, h] = Ui::Gl::intrinsicSizeOf(raw);

        const content_bounds_t cb = Ui::Gl::contentBoundsOf(raw, w, h);

        {
            const std::scoped_lock lock(s_documentsMutex);
            s_documents[id]     = sdoc;
            s_sizes[id]         = { w, h };
            s_contentBounds[id] = cb;
        }

        prom.set_value(sdoc);

        if constexpr (SVG_DEBUG) {
            if (s_loggedIds.insert(id).second) {
                std::cout << "[SvgRenderer] Loaded SVG from string: " << id << " -> " << w << "x" << h << " content=["
                          << cb.u0 << "," << cb.v0 << "," << cb.u1 << "," << cb.v1 << "]" << std::endl;
            }
        }

        {
            const std::scoped_lock lock(s_loadsMutex);
            s_loads.erase(id);
        }

        return id;
    }
};

} // namespace Ui::Gl
