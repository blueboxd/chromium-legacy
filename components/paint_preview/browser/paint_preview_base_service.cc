// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_base_service.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/paint_preview/browser/compositor_utils.h"
#include "components/paint_preview/browser/file_manager.h"
#include "components/paint_preview/browser/paint_preview_client.h"
#include "components/paint_preview/browser/paint_preview_compositor_service_impl.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/paint_preview/browser/jni_headers/PaintPreviewBaseService_jni.h"
#endif  // defined(OS_ANDROID)

namespace paint_preview {

namespace {

const char kPaintPreviewDir[] = "paint_preview";

}  // namespace

PaintPreviewBaseService::PaintPreviewBaseService(
    const base::FilePath& path,
    const std::string& ascii_feature_name,
    std::unique_ptr<PaintPreviewPolicy> policy,
    bool is_off_the_record)
    : policy_(std::move(policy)),
      file_manager_(
          path.AppendASCII(kPaintPreviewDir).AppendASCII(ascii_feature_name)),
      is_off_the_record_(is_off_the_record) {
#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_ref =
      Java_PaintPreviewBaseService_Constructor(
          env, reinterpret_cast<intptr_t>(this));
  java_ref_.Reset(java_ref);
#endif  // defined(OS_ANDROID)
}

PaintPreviewBaseService::~PaintPreviewBaseService() {
#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PaintPreviewBaseService_onDestroy(env, java_ref_);
#endif  // defined(OS_ANDROID)
}

base::Optional<PaintPreviewProto>
PaintPreviewBaseService::GetCapturedPaintPreviewProto(const GURL& url) {
  return base::nullopt;
}

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
  if (policy_ && !policy_->SupportedForContents(web_contents)) {
    std::move(callback).Run(kContentUnsupported, nullptr);
    return;
  }

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

  auto start_time = base::TimeTicks::Now();
  client->CapturePaintPreview(
      params, render_frame_host,
      base::BindOnce(&PaintPreviewBaseService::OnCaptured,
                     weak_ptr_factory_.GetWeakPtr(), start_time,
                     std::move(callback)));
}

std::unique_ptr<PaintPreviewCompositorService>
PaintPreviewBaseService::StartCompositorService(
    base::OnceClosure disconnect_handler) {
  return std::make_unique<PaintPreviewCompositorServiceImpl>(
      CreateCompositorCollection(), std::move(disconnect_handler));
}

void PaintPreviewBaseService::OnCaptured(
    base::TimeTicks start_time,
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
  base::UmaHistogramTimes("Browser.PaintPreview.Capture.TotalCaptureDuration",
                          base::TimeTicks::Now() - start_time);
  std::move(callback).Run(kOk, std::move(proto));
}

}  // namespace paint_preview
