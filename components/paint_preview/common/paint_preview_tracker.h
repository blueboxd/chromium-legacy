// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_PAINT_PREVIEW_TRACKER_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_PAINT_PREVIEW_TRACKER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/common/glyph_usage.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/serial_utils.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "ui/gfx/geometry/rect.h"

namespace paint_preview {

// Tracks metadata for a Paint Preview.
class PaintPreviewTracker {
 public:
  PaintPreviewTracker();
  ~PaintPreviewTracker();

  // Data Collection ----------------------------------------------------------

  // Use base::UnguessableToken as a GUID that identifies the paint preview.
  void SetGuid(base::UnguessableToken guid) { guid_ = guid; }
  base::UnguessableToken Guid() const { return guid_; }

  // Creates a placeholder SkPicture for an OOP subframe located at |rect|
  // mapped to the |routing_id| of OOP RenderFrame. Returns the content id of
  // the placeholder SkPicture.
  uint32_t CreateContentForRemoteFrame(const gfx::Rect& rect, int routing_id);

  // Adds the glyphs in |blob| to the glyph usage tracker for the |blob|'s
  // associated typface.
  void AddGlyphs(const SkTextBlob* blob);

  // Adds |link| with bounding box |rect| to the list of links.
  void AnnotateLink(const std::string& link, const gfx::Rect& rect);

  // Data Serialization -------------------------------------------------------
  // NOTE: once any of these methods are called the PaintPreviewTracker should
  // be considered immutable.

  // Inserts the OOP subframe placeholder associated with |content_id| into
  // |canvas|.
  void CustomDataToSkPictureCallback(SkCanvas* canvas, uint32_t content_id);

  // Expose internal maps for use in MakeSerialProcs().
  PictureSerializationContext* GetPictureSerializationContext() {
    return &content_id_to_proxy_id_;
  }
  TypefaceUsageMap* GetTypefaceUsageMap() { return &typeface_glyph_usage_; }

  // Expose links for serialization to a PaintPreviewFrameProto.
  const std::vector<LinkDataProto>& GetLinks() const { return links_; }

 private:
  base::UnguessableToken guid_;
  std::vector<LinkDataProto> links_;
  PictureSerializationContext content_id_to_proxy_id_;
  TypefaceUsageMap typeface_glyph_usage_;
  base::flat_map<uint32_t, sk_sp<SkPicture>> subframe_pics_;

  DISALLOW_COPY_AND_ASSIGN(PaintPreviewTracker);
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_PAINT_PREVIEW_TRACKER_H_
