// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/paint_preview/services/paint_preview_demo_service.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/paint_preview/browser/file_manager.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/browser/paint_preview_policy.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect.h"
#if defined(OS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/PaintPreviewDemoService_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
#endif  // defined(OS_ANDROID)

namespace paint_preview {

PaintPreviewDemoService::PaintPreviewDemoService(
    const base::FilePath& profile_path,
    bool is_off_the_record)
    : PaintPreviewBaseService(profile_path,
                              "PaintPreviewDemoService",
                              nullptr,
                              is_off_the_record) {
#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ref_.Reset(Java_PaintPreviewDemoService_Constructor(
      env, reinterpret_cast<intptr_t>(this)));
#endif  // defined(OS_ANDROID)
}

PaintPreviewDemoService::~PaintPreviewDemoService() {
#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PaintPreviewDemoService_destroy(env, java_ref_);
  java_ref_.Reset();
#endif  // defined(OS_ANDROID)
}

#if defined(OS_ANDROID)
void PaintPreviewDemoService::CleanUpForTabId(JNIEnv* env, jint tab_id) {
  auto file_manager = GetFileManager();
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&FileManager::DeleteArtifactSet, file_manager,
                                file_manager->CreateKey(tab_id)));
}

void PaintPreviewDemoService::CapturePaintPreviewJni(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_web_contents,
    int tab_id,
    const JavaParamRef<jobject>& j_callback) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  CapturePaintPreview(
      web_contents, tab_id,
      base::BindOnce(
          [](const ScopedJavaGlobalRef<jobject>& java_callback, bool success) {
            base::android::RunBooleanCallbackAndroid(java_callback, success);
          },
          ScopedJavaGlobalRef<jobject>(j_callback)));
}
#endif  // defined(OS_ANDROID)

void PaintPreviewDemoService::CapturePaintPreview(
    content::WebContents* web_contents,
    int tab_id,
    FinishedSuccessfullyCallback callback) {
  DirectoryKey key = GetFileManager()->CreateKey(tab_id);
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::CreateOrGetDirectory, GetFileManager(), key,
                     true),
      base::BindOnce(&PaintPreviewDemoService::CaptureTabInternal,
                     weak_ptr_factory_.GetWeakPtr(),
                     web_contents->GetMainFrame()->GetFrameTreeNodeId(), key,
                     std::move(callback)));
}

void PaintPreviewDemoService::CaptureTabInternal(
    int main_frame_tree_no_id,
    const DirectoryKey& key,
    FinishedSuccessfullyCallback callback,
    const base::Optional<base::FilePath>& file_path) {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(main_frame_tree_no_id);
  if (!file_path.has_value() || !web_contents) {
    std::move(callback).Run(false);
    return;
  }

  PaintPreviewBaseService::CapturePaintPreview(
      web_contents, file_path.value(), gfx::Rect(), /*max_per_capture_size=*/0,
      base::BindOnce(&PaintPreviewDemoService::OnCaptured,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback), key));
}

void PaintPreviewDemoService::OnCaptured(
    FinishedSuccessfullyCallback callback,
    const DirectoryKey& key,
    PaintPreviewBaseService::CaptureStatus status,
    std::unique_ptr<PaintPreviewProto> proto) {
  if (status != PaintPreviewBaseService::CaptureStatus::kOk || !proto) {
    std::move(callback).Run(false);
    return;
  }

  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::SerializePaintPreviewProto, GetFileManager(),
                     key, *proto, true),
      base::BindOnce(&PaintPreviewDemoService::OnSerializationFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaintPreviewDemoService::OnSerializationFinished(
    FinishedSuccessfullyCallback callback,
    bool success) {
  std::move(callback).Run(success);
}
}  // namespace paint_preview
