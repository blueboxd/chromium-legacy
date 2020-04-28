// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/font_access/font_metadata.h"

#include "third_party/blink/renderer/platform/fonts/font_cache.h"

namespace blink {

FontMetadata::FontMetadata(const FontEnumerationEntry& entry)
    : postscriptName_(entry.postscript_name),
      fullName_(entry.full_name),
      family_(entry.family) {}

FontMetadata* FontMetadata::Create(const FontEnumerationEntry& entry) {
  return MakeGarbageCollected<FontMetadata>(entry);
}

void FontMetadata::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
