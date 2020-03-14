// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_DEMO_SERVICE_H_
#define CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_DEMO_SERVICE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/browser/paint_preview_policy.h"
#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#endif  // defined(OS_ANDROID)

namespace content {
class WebContents;
}  // namespace content

namespace paint_preview {

// A PaintPreviewBaseService used for demoing the paint preview
// component.
class PaintPreviewDemoService : public PaintPreviewBaseService {
 public:
  PaintPreviewDemoService(const base::FilePath& profile_dir,
                          bool is_off_the_record);
  ~PaintPreviewDemoService() override;
  PaintPreviewDemoService(const PaintPreviewDemoService&) = delete;
  PaintPreviewDemoService& operator=(const PaintPreviewDemoService&) = delete;

#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> GetJavaRef() { return java_ref_; }

  void CleanUpForTabId(JNIEnv* env, jint tab_id);

  void CapturePaintPreviewJni(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_web_contents,
      jint tab_id,
      const base::android::JavaParamRef<jobject>& j_callback);
#endif  // defined(OS_ANDROID)

  using FinishedSuccessfullyCallback = base::OnceCallback<void(bool)>;
  void CapturePaintPreview(content::WebContents* web_contents,
                           int tab_id,
                           FinishedSuccessfullyCallback callback);

 private:
  void CaptureTabInternal(int main_frame_tree_no_id,
                          const DirectoryKey& key,
                          FinishedSuccessfullyCallback callback,
                          const base::Optional<base::FilePath>& file_path);

  void OnCaptured(FinishedSuccessfullyCallback callback,
                  const DirectoryKey& key,
                  PaintPreviewBaseService::CaptureStatus status,
                  std::unique_ptr<PaintPreviewProto> proto);

  void OnSerializationFinished(FinishedSuccessfullyCallback callback,
                               bool success);

#if defined(OS_ANDROID)
  // The java side PaintPreviewDemoService.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
#endif  // defined(OS_ANDROID)
  base::WeakPtrFactory<PaintPreviewDemoService> weak_ptr_factory_{this};
};

}  // namespace paint_preview

#endif  // CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_DEMO_SERVICE_H_
