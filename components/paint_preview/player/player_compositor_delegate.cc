// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/player_compositor_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/browser/compositor_utils.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/public/paint_preview_compositor_client.h"
#include "components/paint_preview/public/paint_preview_compositor_service.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace paint_preview {
namespace {

base::flat_map<base::UnguessableToken, base::File> CreateFileMapFromProto(
    const paint_preview::PaintPreviewProto& proto) {
  std::vector<std::pair<base::UnguessableToken, base::File>> entries;
  entries.reserve(1 + proto.subframes_size());
  base::UnguessableToken root_frame_id = base::UnguessableToken::Deserialize(
      proto.root_frame().embedding_token_high(),
      proto.root_frame().embedding_token_low());
  base::BasicStringPiece<std::string> root_frame_file_path =
      proto.root_frame().file_path();
  entries.emplace_back(
      root_frame_id, base::File(base::FilePath(root_frame_file_path),
                                base::File::FLAG_OPEN | base::File::FLAG_READ));
  for (const auto& subframe : proto.subframes()) {
    base::BasicStringPiece<std::string> frame_file_path = subframe.file_path();
    entries.emplace_back(
        base::UnguessableToken::Deserialize(subframe.embedding_token_high(),
                                            subframe.embedding_token_low()),
        base::File(base::FilePath(frame_file_path),
                   base::File::FLAG_OPEN | base::File::FLAG_READ));
  }
  return base::flat_map<base::UnguessableToken, base::File>(std::move(entries));
}

base::Optional<base::ReadOnlySharedMemoryRegion> ToReadOnlySharedMemory(
    const paint_preview::PaintPreviewProto& proto) {
  auto region = base::WritableSharedMemoryRegion::Create(proto.ByteSizeLong());
  if (!region.IsValid())
    return base::nullopt;

  auto mapping = region.Map();
  if (!mapping.IsValid())
    return base::nullopt;

  proto.SerializeToArray(mapping.memory(), mapping.size());
  return base::WritableSharedMemoryRegion::ConvertToReadOnly(std::move(region));
}
}  // namespace

PlayerCompositorDelegate::PlayerCompositorDelegate(
    PaintPreviewBaseService* paint_preview_service,
    const GURL& url)
    : paint_preview_service_(paint_preview_service) {
  paint_preview_compositor_service_ =
      paint_preview_service_->StartCompositorService(base::BindOnce(
          &PlayerCompositorDelegate::OnCompositorServiceDisconnected,
          weak_factory_.GetWeakPtr()));
  paint_preview_compositor_client_ =
      paint_preview_compositor_service_->CreateCompositor(
          base::BindOnce(&PlayerCompositorDelegate::OnCompositorClientCreated,
                         weak_factory_.GetWeakPtr(), url));
  paint_preview_compositor_client_->SetDisconnectHandler(
      base::BindOnce(&PlayerCompositorDelegate::OnCompositorClientDisconnected,
                     weak_factory_.GetWeakPtr()));
}

void PlayerCompositorDelegate::OnCompositorServiceDisconnected() {
  // TODO(crbug.com/1039699): Handle compositor service disconnect event.
}

void PlayerCompositorDelegate::OnCompositorClientCreated(const GURL& url) {
  paint_preview_compositor_client_->SetRootFrameUrl(url);

  base::Optional<PaintPreviewProto> proto =
      paint_preview_service_->GetCapturedPaintPreviewProto(url);
  if (!proto || !proto.value().IsInitialized()) {
    // TODO(crbug.com/1021590): Handle initialization errors.
    return;
  }

  // TODO(crbug.com/1034111): Investigate executing this in the background.
  mojom::PaintPreviewBeginCompositeRequestPtr begin_composite_request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  begin_composite_request->file_map = CreateFileMapFromProto(proto.value());
  // TODO(crbug.com/1034111): Don't perform this on UI thread.
  auto read_only_proto = ToReadOnlySharedMemory(proto.value());
  if (!read_only_proto) {
    // TODO(crbug.com/1021590): Handle initialization errors.
    return;
  }
  begin_composite_request->proto = std::move(read_only_proto.value());
  paint_preview_compositor_client_->BeginComposite(
      std::move(begin_composite_request),
      base::BindOnce(&PlayerCompositorDelegate::OnCompositorReady,
                     weak_factory_.GetWeakPtr()));
  // TODO(crbug.com/1019883): Initialize the HitTester class.
}

void PlayerCompositorDelegate::OnCompositorClientDisconnected() {
  // TODO(crbug.com/1039699): Handle compositor client disconnect event.
}

void PlayerCompositorDelegate::RequestBitmap(
    const base::UnguessableToken& frame_guid,
    const gfx::Rect& clip_rect,
    float scale_factor,
    base::OnceCallback<void(mojom::PaintPreviewCompositor::Status,
                            const SkBitmap&)> callback) {
  if (!paint_preview_compositor_client_) {
    std::move(callback).Run(
        mojom::PaintPreviewCompositor::Status::kCompositingFailure, SkBitmap());
    return;
  }

  paint_preview_compositor_client_->BitmapForFrame(
      frame_guid, clip_rect, scale_factor, std::move(callback));
}

void PlayerCompositorDelegate::OnClick(const base::UnguessableToken& frame_guid,
                                       int x,
                                       int y) {
  // TODO(crbug.com/1019883): Handle url clicks with the HitTester class.
}

PlayerCompositorDelegate::~PlayerCompositorDelegate() = default;

}  // namespace paint_preview
