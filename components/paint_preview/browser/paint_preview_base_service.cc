// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_base_service.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/paint_preview/browser/file_manager.h"
#include "components/paint_preview/browser/paint_preview_client.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "content/public/browser/web_contents.h"

namespace paint_preview {

namespace {

const char kPaintPreviewDir[] = "paint_preview";

}  // namespace

PaintPreviewBaseService::PaintPreviewBaseService(
    const base::FilePath& path,
    const std::string& ascii_feature_name,
    bool is_off_the_record)
    : file_manager_(
          path.AppendASCII(kPaintPreviewDir).AppendASCII(ascii_feature_name)),
      is_off_the_record_(is_off_the_record) {}
PaintPreviewBaseService::~PaintPreviewBaseService() = default;

void PaintPreviewBaseService::CapturePaintPreview(
    content::WebContents* web_contents,
    const base::FilePath& root_dir,
    gfx::Rect clip_rect,
    OnCapturedCallback callback) {
  CapturePaintPreview(web_contents, web_contents->GetMainFrame(), root_dir,
                      clip_rect, std::move(callback));
}

void PaintPreviewBaseService::CapturePaintPreview(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    const base::FilePath& root_dir,
    gfx::Rect clip_rect,
    OnCapturedCallback callback) {
  PaintPreviewClient::CreateForWebContents(web_contents);  // Is a singleton.
  auto* client = PaintPreviewClient::FromWebContents(web_contents);
  if (!client) {
    std::move(callback).Run(kClientCreationFailed, nullptr);
    return;
  }

  PaintPreviewClient::PaintPreviewParams params;
  params.document_guid = base::UnguessableToken::Create();
  params.clip_rect = clip_rect;
  params.is_main_frame = (render_frame_host == web_contents->GetMainFrame());
  params.root_dir = root_dir;

  client->CapturePaintPreview(
      params, render_frame_host,
      base::BindOnce(&PaintPreviewBaseService::OnCaptured,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaintPreviewBaseService::OnCaptured(
    OnCapturedCallback callback,
    base::UnguessableToken guid,
    mojom::PaintPreviewStatus status,
    std::unique_ptr<PaintPreviewProto> proto) {
  if (status != mojom::PaintPreviewStatus::kOk) {
    DVLOG(1) << "ERROR: Paint Preview failed to capture for document "
             << guid.ToString() << " with error " << status;
    std::move(callback).Run(kCaptureFailed, nullptr);
    return;
  }
  std::move(callback).Run(kOk, std::move(proto));
}

}  // namespace paint_preview
