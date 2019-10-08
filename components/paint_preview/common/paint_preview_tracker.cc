// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/paint_preview_tracker.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/paint_preview/common/glyph_usage.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/geometry/rect.h"

namespace paint_preview {

namespace {

constexpr int kMaxGlyphsForDenseGlyphUsage = 10000;

// Heuristically choose between a dense and sparse glyph usage map.
// TODO(crbug/1009538): Gather data to make this heuristic better.
bool ShouldUseDenseGlyphUsage(SkTypeface* typeface) {
  // DenseGlyphUsage is a bitset; it is efficient if lots of glyphs are used.
  // SparseGlyphUsage is a set; it is efficient if few glyphs are used.
  // Generally, smaller fonts have a higher percentage of used glyphs so set a
  // maximum threshold for number of glyphs before using SparseGlyphUsage.
  return typeface->countGlyphs() < kMaxGlyphsForDenseGlyphUsage;
}

void RectToRectProto(RectProto* rect_proto, const gfx::Rect& rect) {
  rect_proto->set_x(rect.x());
  rect_proto->set_y(rect.y());
  rect_proto->set_width(rect.width());
  rect_proto->set_height(rect.height());
}

}  // namespace

PaintPreviewTracker::PaintPreviewTracker() = default;
PaintPreviewTracker::~PaintPreviewTracker() = default;

uint32_t PaintPreviewTracker::CreateContentForRemoteFrame(const gfx::Rect& rect,
                                                          int routing_id) {
  sk_sp<SkPicture> pic = SkPicture::MakePlaceholder(
      SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()));
  const uint32_t content_id = pic->uniqueID();
  DCHECK(!base::Contains(content_id_to_proxy_id_, content_id));
  content_id_to_proxy_id_[content_id] = routing_id;
  subframe_pics_[content_id] = pic;
  return content_id;
}

void PaintPreviewTracker::AddGlyphs(const SkTextBlob* blob) {
  if (!blob)
    return;
  SkTextBlob::Iter::Run run;
  for (SkTextBlob::Iter it(*blob); it.next(&run);) {
    SkTypeface* typeface = run.fTypeface;
    // Fail fast if the number of glyphs is undetermined or 0.
    if (typeface->countGlyphs() <= 0)
      continue;
    if (!base::Contains(typeface_glyph_usage_, typeface->uniqueID())) {
      if (ShouldUseDenseGlyphUsage(typeface)) {
        typeface_glyph_usage_.insert(
            {typeface->uniqueID(),
             std::make_unique<DenseGlyphUsage>(
                 static_cast<uint16_t>(typeface->countGlyphs() - 1))});
      } else {
        typeface_glyph_usage_.insert(
            {typeface->uniqueID(),
             std::make_unique<SparseGlyphUsage>(
                 static_cast<uint16_t>(typeface->countGlyphs() - 1))});
      }
      // Always set the 0th glyph.
      typeface_glyph_usage_[typeface->uniqueID()]->Set(0U);
    }
    const uint16_t* glyphs = run.fGlyphIndices;
    for (int i = 0; i < run.fGlyphCount; ++i)
      typeface_glyph_usage_[typeface->uniqueID()]->Set(glyphs[i]);
  }
}

void PaintPreviewTracker::AnnotateLink(const std::string& url,
                                       const gfx::Rect& rect) {
  LinkDataProto link_data;
  RectToRectProto(link_data.mutable_rect(), rect);
  link_data.set_url(url);
  links_.push_back(link_data);
}

void PaintPreviewTracker::CustomDataToSkPictureCallback(SkCanvas* canvas,
                                                        uint32_t content_id) {
  auto map_it = content_id_to_proxy_id_.find(content_id);
  if (map_it == content_id_to_proxy_id_.end())
    return;

  auto it = subframe_pics_.find(content_id);
  // DCHECK is sufficient as |subframe_pics_| has same entries as
  // |content_id_to_proxy_id_|.
  DCHECK(it != subframe_pics_.end());

  SkRect rect = it->second->cullRect();
  SkMatrix matrix = SkMatrix::MakeTrans(rect.x(), rect.y());
  canvas->drawPicture(it->second, &matrix, nullptr);
}

}  // namespace paint_preview
