// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_SERIAL_UTILS_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_SERIAL_UTILS_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/common/glyph_usage.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/geometry/rect.h"

namespace paint_preview {

struct PictureSerializationContext {
  PictureSerializationContext();
  ~PictureSerializationContext();

  PictureSerializationContext(const PictureSerializationContext&) = delete;
  PictureSerializationContext& operator=(const PictureSerializationContext&) =
      delete;

  PictureSerializationContext(PictureSerializationContext&&) noexcept;
  PictureSerializationContext& operator=(
      PictureSerializationContext&&) noexcept;

  // Maps a content ID to a transformed clip rect.
  base::flat_map<uint32_t, SkRect> content_id_to_transformed_clip;

  // Maps a content ID to an embedding token.
  base::flat_map<uint32_t, base::UnguessableToken>
      content_id_to_embedding_token;
};

// Maps a typeface ID to a glyph usage tracker.
using TypefaceUsageMap = base::flat_map<SkFontID, std::unique_ptr<GlyphUsage>>;

// Tracks typeface deduplication and handles subsetting.
struct TypefaceSerializationContext {
  TypefaceSerializationContext(TypefaceUsageMap* usage);
  ~TypefaceSerializationContext();

  TypefaceUsageMap* usage;
  base::flat_set<SkFontID> finished;  // Should be empty on first use.
};

// Maps a content ID to a clip rect.
using DeserializationContext = base::flat_map<uint32_t, gfx::Rect>;

// A pair that contains a frame's |SkPicture| and its associated scroll offsets.
// Used in |LoadedFramesDeserialContext| to correctly replay the scroll state
// for subframes.
struct FrameAndScrollOffsets {
  FrameAndScrollOffsets();
  ~FrameAndScrollOffsets();

  FrameAndScrollOffsets(const FrameAndScrollOffsets&);
  FrameAndScrollOffsets& operator=(const FrameAndScrollOffsets&);

  sk_sp<SkPicture> picture;
  gfx::Size scroll_offsets;
};

// Maps a content ID to a frame's picture. A frame's subframes should be
// loaded into this context before |MakeDeserialProcs| is called to ensure
// that the resulting |SkPicture| contains all subframes.
using LoadedFramesDeserialContext =
    base::flat_map<uint32_t, FrameAndScrollOffsets>;

// Creates a no-op SkPicture.
sk_sp<SkPicture> MakeEmptyPicture();

// Creates a SkSerialProcs object. The object *does not* copy |picture_ctx| or
// |typeface_ctx| so they must outlive the use of the returned object.
SkSerialProcs MakeSerialProcs(PictureSerializationContext* picture_ctx,
                              TypefaceSerializationContext* typeface_ctx);

// Creates a SkDeserialProcs object. The object *does not* copy |ctx| so |ctx|
// must outlive the use of the returned object. |ctx| will be filled as pictures
// are being deserialized. Subframes will be filled with |MakeEmptyPicture|.
SkDeserialProcs MakeDeserialProcs(DeserializationContext* ctx);

// Creates a SkDeserialProcs object. The object *does not* copy |ctx| so |ctx|
// must outlive the use of the returned object. |ctx| will be referenced for
// subframes as pictures are being serialized. If a subframe does not exist in
// |ctx|, its rectangle will be fillde with |MakeEmptyPicture|.
SkDeserialProcs MakeDeserialProcs(LoadedFramesDeserialContext* ctx);

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_SERIAL_UTILS_H_
