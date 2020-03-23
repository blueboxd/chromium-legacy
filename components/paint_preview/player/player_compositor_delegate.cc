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
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
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

std::pair<base::UnguessableToken, std::unique_ptr<HitTester>> BuildHitTester(
    const PaintPreviewFrameProto& proto) {
  std::pair<base::UnguessableToken, std::unique_ptr<HitTester>> out(
      base::UnguessableToken::Deserialize(proto.embedding_token_high(),
                                          proto.embedding_token_low()),
      std::make_unique<HitTester>());
  out.second->Build(proto);
  return out;
}

base::flat_map<base::UnguessableToken, std::unique_ptr<HitTester>>
BuildHitTesters(const PaintPreviewProto& proto) {
  std::vector<std::pair<base::UnguessableToken, std::unique_ptr<HitTester>>>
      hit_testers;
  hit_testers.reserve(proto.subframes_size() + 1);
  hit_testers.push_back(BuildHitTester(proto.root_frame()));
  for (const auto& frame_proto : proto.subframes())
    hit_testers.push_back(BuildHitTester(frame_proto));

  return base::flat_map<base::UnguessableToken, std::unique_ptr<HitTester>>(
      std::move(hit_testers));
}

base::FilePath ToFilePath(base::StringPiece path_str) {
#if defined(OS_WIN)
  return base::FilePath(base::UTF8ToUTF16(path_str));
#else
  return base::FilePath(path_str);
#endif
}

base::flat_map<base::UnguessableToken, base::File> CreateFileMapFromProto(
    const PaintPreviewProto& proto) {
  std::vector<std::pair<base::UnguessableToken, base::File>> entries;
  entries.reserve(1 + proto.subframes_size());
  base::UnguessableToken root_frame_id = base::UnguessableToken::Deserialize(
      proto.root_frame().embedding_token_high(),
      proto.root_frame().embedding_token_low());
  base::File root_frame_skp_file =
      base::File(ToFilePath(proto.root_frame().file_path()),
                 base::File::FLAG_OPEN | base::File::FLAG_READ);

  // We can't composite anything with an invalid SKP file path for the root
  // frame.
  if (!root_frame_skp_file.IsValid())
    return base::flat_map<base::UnguessableToken, base::File>();

  entries.emplace_back(std::move(root_frame_id),
                       std::move(root_frame_skp_file));
  for (const auto& subframe : proto.subframes()) {
    base::File frame_skp_file(ToFilePath(subframe.file_path()),
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
    const GURL& expected_url,
    const DirectoryKey& key,
    bool skip_service_launch)
    : paint_preview_service_(paint_preview_service) {
  if (skip_service_launch) {
    paint_preview_service_->GetCapturedPaintPreviewProto(
        key, base::BindOnce(&PlayerCompositorDelegate::OnProtoAvailable,
                            weak_factory_.GetWeakPtr(), expected_url));
    return;
  }
  paint_preview_compositor_service_ =
      paint_preview_service_->StartCompositorService(base::BindOnce(
          &PlayerCompositorDelegate::OnCompositorServiceDisconnected,
          weak_factory_.GetWeakPtr()));

  paint_preview_compositor_client_ =
      paint_preview_compositor_service_->CreateCompositor(
          base::BindOnce(&PlayerCompositorDelegate::OnCompositorClientCreated,
                         weak_factory_.GetWeakPtr(), expected_url, key));
  paint_preview_compositor_client_->SetDisconnectHandler(
      base::BindOnce(&PlayerCompositorDelegate::OnCompositorClientDisconnected,
                     weak_factory_.GetWeakPtr()));
}

PlayerCompositorDelegate::~PlayerCompositorDelegate() = default;

void PlayerCompositorDelegate::OnCompositorServiceDisconnected() {
  // TODO(crbug.com/1039699): Handle compositor service disconnect event.
}

void PlayerCompositorDelegate::OnCompositorClientCreated(
    const GURL& expected_url,
    const DirectoryKey& key) {
  paint_preview_service_->GetCapturedPaintPreviewProto(
      key, base::BindOnce(&PlayerCompositorDelegate::OnProtoAvailable,
                          weak_factory_.GetWeakPtr(), expected_url));
}

void PlayerCompositorDelegate::OnProtoAvailable(
    const GURL& expected_url,
    std::unique_ptr<PaintPreviewProto> proto) {
  if (!proto || !proto->IsInitialized()) {
    // TODO(crbug.com/1021590): Handle initialization errors.
    OnCompositorReady(
        mojom::PaintPreviewCompositor::Status::kCompositingFailure, nullptr);
    return;
  }

  auto proto_url = GURL(proto->metadata().url());
  if (expected_url != proto_url) {
    OnCompositorReady(
        mojom::PaintPreviewCompositor::Status::kDeserializingFailure, nullptr);
    return;
  }

  hit_testers_ = BuildHitTesters(*proto);

  if (!paint_preview_compositor_client_) {
    OnCompositorReady(
        mojom::PaintPreviewCompositor::Status::kCompositingFailure, nullptr);
    return;
  }

  paint_preview_compositor_client_->SetRootFrameUrl(proto_url);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&PrepareCompositeRequest, *proto),
      base::BindOnce(&PlayerCompositorDelegate::SendCompositeRequest,
                     weak_factory_.GetWeakPtr()));
}

void PlayerCompositorDelegate::SendCompositeRequest(
    mojom::PaintPreviewBeginCompositeRequestPtr begin_composite_request) {
  // TODO(crbug.com/1021590): Handle initialization errors.
  if (!begin_composite_request) {
    OnCompositorReady(
        mojom::PaintPreviewCompositor::Status::kCompositingFailure, nullptr);
    return;
  }

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

std::vector<const GURL*> PlayerCompositorDelegate::OnClick(
    const base::UnguessableToken& frame_guid,
    const gfx::Rect& rect) {
  std::vector<const GURL*> urls;
  auto it = hit_testers_.find(frame_guid);
  if (it != hit_testers_.end())
    it->second->HitTest(rect, &urls);
  return urls;
}

}  // namespace paint_preview
