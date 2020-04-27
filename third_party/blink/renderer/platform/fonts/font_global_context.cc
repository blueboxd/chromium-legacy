// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_global_context.h"

#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_font_cache.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

FontGlobalContext* FontGlobalContext::Get(CreateIfNeeded create_if_needed) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<FontGlobalContext*>,
                                  font_persistent, ());
  if (!*font_persistent && create_if_needed == kCreate) {
    *font_persistent = new FontGlobalContext();
  }
  return *font_persistent;
}

FontGlobalContext::FontGlobalContext()
    : harfbuzz_font_funcs_skia_advances_(nullptr),
      harfbuzz_font_funcs_harfbuzz_advances_(nullptr) {}

FontGlobalContext::~FontGlobalContext() = default;

FontUniqueNameLookup* FontGlobalContext::GetFontUniqueNameLookup() {
  if (!Get()->font_unique_name_lookup_) {
    Get()->font_unique_name_lookup_ =
        FontUniqueNameLookup::GetPlatformUniqueNameLookup();
  }
  return Get()->font_unique_name_lookup_.get();
}

HarfBuzzFontCache* FontGlobalContext::GetHarfBuzzFontCache() {
  std::unique_ptr<HarfBuzzFontCache>& global_context_harfbuzz_font_cache =
      Get()->harfbuzz_font_cache_;
  if (!global_context_harfbuzz_font_cache) {
    global_context_harfbuzz_font_cache = std::make_unique<HarfBuzzFontCache>();
  }
  return global_context_harfbuzz_font_cache.get();
}

void FontGlobalContext::ClearMemory() {
  if (!Get(kDoNotCreate))
    return;

  GetFontCache().Invalidate();
}

}  // namespace blink
