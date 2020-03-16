// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_TAB_SERVICE_H_
#define CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_TAB_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/browser/paint_preview_policy.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"

#if defined(os_android)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#endif  // defined(os_android)

namespace content {
class WebContents;
}  // namespace content

namespace paint_preview {

// A service for capturing and using Paint Previews per Tab. Captures are stored
// using Tab IDs as the key such that the data can be accessed even if the
// browser is restarted.
class PaintPreviewTabService : public PaintPreviewBaseService {
 public:
  PaintPreviewTabService(const base::FilePath& profile_dir,
                         base::StringPiece ascii_feature_name,
                         std::unique_ptr<PaintPreviewPolicy> policy,
                         bool is_off_the_record);
  ~PaintPreviewTabService() override;

  enum Status {
    kOk = 0,
    kDirectoryCreationFailed = 1,
    kCaptureFailed = 2,
    kProtoSerializationFailed = 3,
    kWebContentsGone = 4,
  };

  using FinishedCallback = base::OnceCallback<void(Status)>;
  using BooleanCallback = base::OnceCallback<void(bool)>;

  // Captures a Paint Preview of |contents| which should be associated with
  // |tab_id| for storage. |callback| is invoked on completion to indicate
  // status.
  void CaptureTab(int tab_id,
                  content::WebContents* contents,
                  FinishedCallback callback);

  // Destroys the Paint Preview associated with |tab_id|. This MUST be called
  // when a tab is closed to ensure the captured contents don't outlive the tab.
  void TabClosed(int tab_id);

  // Checks if there is a capture taken for |tab_id|.
  void HasCaptureForTab(int tab_id, BooleanCallback callback);

  // This should be called on startup with a list of restored tab ids
  // (|active_tab_ids|). This performs an audit over all Paint Previews stored
  // by this service and destroys any that don't correspond to active tabs. This
  // is required as TabClosed could have been interrupted or an accounting error
  // occurred.
  void AuditArtifacts(const std::vector<int>& active_tab_ids);

#if defined(OS_ANDROID)
  // JNI wrapped versions of the above methods
  void CaptureTab(JNIEnv* env,
                  jint j_tab_id,
                  const base::android::JavaParamRef<jobject>& j_web_contents,
                  const base::android::JavaParamRef<jobject>& j_callback);
  void TabClosed(JNIEnv* env, jint j_tab_id);
  void HasCaptureForTab(JNIEnv* env,
                        jint j_tab_id,
                        const base::android::JavaParamRef<jobject>& j_callback);
  void AuditArtifacts(JNIEnv* env,
                      const base::android::JavaParamRef<jintArray>& j_tab_ids);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaRef() { return java_ref_; }
#endif  // defined(OS_ANDROID)

 private:
  void CaptureTabInternal(const DirectoryKey& key,
                          int frame_tree_node_id,
                          FinishedCallback callback,
                          const base::Optional<base::FilePath>& file_path);

  void OnCaptured(const DirectoryKey& key,
                  FinishedCallback callback,
                  PaintPreviewBaseService::CaptureStatus status,
                  std::unique_ptr<PaintPreviewProto>);

  void OnFinished(FinishedCallback callback, bool success);

  void RunAudit(const std::vector<int>& active_tab_ids,
                const base::flat_set<DirectoryKey>& in_use_keys);

#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
#endif  // defined(OS_ANDROID)
  base::WeakPtrFactory<PaintPreviewTabService> weak_ptr_factory_{this};
};

}  // namespace paint_preview

#endif  // CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_TAB_SERVICE_H_
