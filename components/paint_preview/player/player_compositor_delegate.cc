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
#include "base/task/post_task.h"
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

namespace paint_preview {
namespace {

base::flat_map<base::UnguessableToken, base::File> CreateFileMapFromProto(
    const paint_preview::PaintPreviewProto& proto) {
  std::vector<std::pair<base::UnguessableToken, base::File>> entries;
  entries.reserve(1 + proto.subframes_size());
  base::UnguessableToken root_frame_id = base::UnguessableToken::Deserialize(
      proto.root_frame().embedding_token_high(),
      proto.root_frame().embedding_token_low());
  base::File root_frame_skp_file =
      base::File(base::FilePath(proto.root_frame().file_path()),
                 base::File::FLAG_OPEN | base::File::FLAG_READ);

  // We can't composite anything with an invalid SKP file path for the root
  // frame.
  if (!root_frame_skp_file.IsValid())
    return base::flat_map<base::UnguessableToken, base::File>();

  entries.emplace_back(std::move(root_frame_id),
                       std::move(root_frame_skp_file));
  for (const auto& subframe : proto.subframes()) {
    base::File frame_skp_file(base::FilePath(subframe.file_path()),
                              base::File::FLAG_OPEN | base::File::FLAG_READ);

    // Skip this frame if it doesn't have a valid SKP file path.
    if (!frame_skp_file.IsValid())
      continue;

    entries.emplace_back(
        base::UnguessableToken::Deserialize(subframe.embedding_token_high(),
                                            subframe.embedding_token_low()),
        std::move(frame_skp_file));
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

paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
PrepareCompositeRequest(const paint_preview::PaintPreviewProto& proto) {
  paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
      begin_composite_request =
          paint_preview::mojom::PaintPreviewBeginCompositeRequest::New();
  begin_composite_request->file_map = CreateFileMapFromProto(proto);
  if (begin_composite_request->file_map.empty())
    return nullptr;

  auto read_only_proto = ToReadOnlySharedMemory(proto);
  if (!read_only_proto) {
    // TODO(crbug.com/1021590): Handle initialization errors.
    return nullptr;
  }
  begin_composite_request->proto = std::move(read_only_proto.value());
  return begin_composite_request;
}
}  // namespace

PlayerCompositorDelegate::PlayerCompositorDelegate(
    PaintPreviewBaseService* paint_preview_service,
    const DirectoryKey& key)
    : paint_preview_service_(paint_preview_service) {
  paint_preview_compositor_service_ =
      paint_preview_service_->StartCompositorService(base::BindOnce(
          &PlayerCompositorDelegate::OnCompositorServiceDisconnected,
          weak_factory_.GetWeakPtr()));

  paint_preview_compositor_client_ =
      paint_preview_compositor_service_->CreateCompositor(
          base::BindOnce(&PlayerCompositorDelegate::OnCompositorClientCreated,
                         weak_factory_.GetWeakPtr(), key));
  paint_preview_compositor_client_->SetDisconnectHandler(
      base::BindOnce(&PlayerCompositorDelegate::OnCompositorClientDisconnected,
                     weak_factory_.GetWeakPtr()));
}

void PlayerCompositorDelegate::OnCompositorServiceDisconnected() {
  // TODO(crbug.com/1039699): Handle compositor service disconnect event.
}

void PlayerCompositorDelegate::OnCompositorClientCreated(
    const DirectoryKey& key) {
  paint_preview_service_->GetCapturedPaintPreviewProto(
      key, base::BindOnce(&PlayerCompositorDelegate::OnProtoAvailable,
                          weak_factory_.GetWeakPtr()));
}

void PlayerCompositorDelegate::OnProtoAvailable(
    std::unique_ptr<PaintPreviewProto> proto) {
  if (!proto || !proto->IsInitialized()) {
    // TODO(crbug.com/1021590): Handle initialization errors.
    return;
  }
  paint_preview_compositor_client_->SetRootFrameUrl(
      GURL(proto->metadata().url()));

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&PrepareCompositeRequest, *proto),
      base::BindOnce(&PlayerCompositorDelegate::SendCompositeRequest,
                     weak_factory_.GetWeakPtr()));
  // TODO(crbug.com/1019883): Initialize the HitTester class.
}

void PlayerCompositorDelegate::SendCompositeRequest(
    mojom::PaintPreviewBeginCompositeRequestPtr begin_composite_request) {
  // TODO(crbug.com/1021590): Handle initialization errors.
  if (!begin_composite_request)
    return;

  paint_preview_compositor_client_->BeginComposite(
      std::move(begin_composite_request),
      base::BindOnce(&PlayerCompositorDelegate::OnCompositorReady,
                     weak_factory_.GetWeakPtr()));
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
