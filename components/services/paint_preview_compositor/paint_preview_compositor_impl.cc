// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/paint_preview_compositor/paint_preview_compositor_impl.h"

#include <utility>

#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "components/services/paint_preview_compositor/skp_result.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkMatrix.h"

namespace paint_preview {

namespace {

base::flat_map<base::UnguessableToken, SkpResult> DeserializeAllFrames(
    base::flat_map<base::UnguessableToken, base::File>* file_map) {
  std::vector<std::pair<base::UnguessableToken, SkpResult>> results;
  results.reserve(file_map->size());

  for (auto& it : *file_map) {
    if (!it.second.IsValid())
      continue;

    SkpResult result;
    FileRStream rstream(std::move(it.second));
    SkDeserialProcs procs = MakeDeserialProcs(&result.ctx);
    result.skp = SkPicture::MakeFromStream(&rstream, &procs);
    if (!result.skp || result.skp->cullRect().width() == 0 ||
        result.skp->cullRect().height() == 0) {
      continue;
    }

    results.push_back({it.first, std::move(result)});
  }

  return base::flat_map<base::UnguessableToken, SkpResult>(std::move(results));
}

base::Optional<PaintPreviewFrame> BuildFrame(
    const base::UnguessableToken& token,
    const PaintPreviewFrameProto& frame_proto,
    const base::flat_map<base::UnguessableToken, SkpResult>& results) {
  auto it = results.find(token);
  if (it == results.end())
    return base::nullopt;

  const SkpResult& result = it->second;
  PaintPreviewFrame frame;
  frame.skp = result.skp;

  for (const auto& id_pair : frame_proto.content_id_to_embedding_tokens()) {
    // It is possible that subframes recorded in this map were not captured
    // (e.g. renderer crash, closed, etc.). Missing subframes are allowable
    // since having just the main frame is sufficient to create a preview.
    auto rect_it = result.ctx.find(id_pair.content_id());
    if (rect_it == result.ctx.end())
      continue;

    mojom::SubframeClipRect rect;
    rect.frame_guid = base::UnguessableToken::Deserialize(
        id_pair.embedding_token_high(), id_pair.embedding_token_low());
    rect.clip_rect = rect_it->second;

    if (!results.count(rect.frame_guid))
      continue;

    frame.subframe_clip_rects.push_back(rect);
  }
  return frame;
}

}  // namespace

PaintPreviewCompositorImpl::PaintPreviewCompositorImpl(
    mojo::PendingReceiver<mojom::PaintPreviewCompositor> receiver,
    base::OnceClosure disconnect_handler) {
  if (receiver) {
    receiver_.Bind(std::move(receiver));
    receiver_.set_disconnect_handler(std::move(disconnect_handler));
  }
}

PaintPreviewCompositorImpl::~PaintPreviewCompositorImpl() {
  receiver_.reset();
}

void PaintPreviewCompositorImpl::BeginComposite(
    mojom::PaintPreviewBeginCompositeRequestPtr request,
    BeginCompositeCallback callback) {
  TRACE_EVENT0("paint_preview", "PaintPreviewCompositorImpl::BeginComposite");
  auto response = mojom::PaintPreviewBeginCompositeResponse::New();
  auto mapping = request->proto.Map();
  if (!mapping.IsValid()) {
    std::move(callback).Run(
        mojom::PaintPreviewCompositor::Status::kDeserializingFailure,
        std::move(response));
    return;
  }

  PaintPreviewProto paint_preview;
  bool ok = paint_preview.ParseFromArray(mapping.memory(), mapping.size());
  if (!ok) {
    DVLOG(1) << "Failed to parse proto.";
    std::move(callback).Run(
        mojom::PaintPreviewCompositor::Status::kDeserializingFailure,
        std::move(response));
    return;
  }
  auto frames = DeserializeAllFrames(&request->file_map);

  // Adding the root frame must succeed.
  if (!AddFrame(paint_preview.root_frame(), frames, &response)) {
    DVLOG(1) << "Root frame not found.";
    std::move(callback).Run(
        mojom::PaintPreviewCompositor::Status::kCompositingFailure,
        std::move(response));
    return;
  }
  response->root_frame_guid = base::UnguessableToken::Deserialize(
      paint_preview.root_frame().embedding_token_high(),
      paint_preview.root_frame().embedding_token_low());

  // Adding subframes is optional.
  for (const auto& subframe_proto : paint_preview.subframes())
    AddFrame(subframe_proto, frames, &response);

  std::move(callback).Run(mojom::PaintPreviewCompositor::Status::kSuccess,
                          std::move(response));
}

void PaintPreviewCompositorImpl::BitmapForFrame(
    const base::UnguessableToken& frame_guid,
    const gfx::Rect& clip_rect,
    float scale_factor,
    BitmapForFrameCallback callback) {
  TRACE_EVENT0("paint_preview", "PaintPreviewCompositorImpl::BitmapForFrame");
  SkBitmap bitmap;
  auto frame_it = frames_.find(frame_guid);
  if (frame_it == frames_.end()) {
    DVLOG(1) << "Frame not found for " << frame_guid.ToString();
    std::move(callback).Run(
        mojom::PaintPreviewCompositor::Status::kCompositingFailure, bitmap);
    return;
  }

  auto skp = frame_it->second.skp;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(clip_rect.width(), clip_rect.height()));
  SkCanvas canvas(bitmap);
  SkMatrix matrix;
  matrix.setScaleTranslate(scale_factor, scale_factor, -clip_rect.x(),
                           -clip_rect.y());
  canvas.drawPicture(skp, &matrix, nullptr);

  std::move(callback).Run(mojom::PaintPreviewCompositor::Status::kSuccess,
                          bitmap);
}

void PaintPreviewCompositorImpl::SetRootFrameUrl(const GURL& url) {
  url_ = url;
}

bool PaintPreviewCompositorImpl::AddFrame(
    const PaintPreviewFrameProto& frame_proto,
    const base::flat_map<base::UnguessableToken, SkpResult>& skp_map,
    mojom::PaintPreviewBeginCompositeResponsePtr* response) {
  base::UnguessableToken guid = base::UnguessableToken::Deserialize(
      frame_proto.embedding_token_high(), frame_proto.embedding_token_low());

  base::Optional<PaintPreviewFrame> maybe_frame =
      BuildFrame(guid, frame_proto, skp_map);
  if (!maybe_frame.has_value())
    return false;
  const PaintPreviewFrame& frame = maybe_frame.value();

  auto frame_data = mojom::FrameData::New();
  SkRect sk_rect = frame.skp->cullRect();
  frame_data->scroll_extents = gfx::Size(sk_rect.width(), sk_rect.height());
  frame_data->subframes.reserve(frame.subframe_clip_rects.size());
  for (const auto& subframe_clip_rect : frame.subframe_clip_rects)
    frame_data->subframes.push_back(subframe_clip_rect.Clone());

  (*response)->frames.insert({guid, std::move(frame_data)});
  frames_.insert({guid, std::move(maybe_frame.value())});
  return true;
}

}  // namespace paint_preview
