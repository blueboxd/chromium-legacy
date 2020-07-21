// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_JAVATESTS_PAINT_PREVIEW_TEST_SERVICE_H_
#define COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_JAVATESTS_PAINT_PREVIEW_TEST_SERVICE_H_

#include "base/files/file_path.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"

namespace paint_preview {

// A simple implementation of PaintPreviewBaseService used in tests.
class PaintPreviewTestService : public PaintPreviewBaseService {
 public:
  PaintPreviewTestService(const base::FilePath& path);
  ~PaintPreviewTestService() override;

  PaintPreviewTestService(const PaintPreviewTestService&) = delete;
  PaintPreviewTestService& operator=(const PaintPreviewTestService&) = delete;

  jboolean CreateSingleSkpForKey(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_key,
      const base::android::JavaParamRef<jstring>& j_url,
      jint j_width,
      jint j_height,
      const base::android::JavaParamRef<jintArray>& j_link_rects,
      const base::android::JavaParamRef<jobjectArray>& j_link_urls);

 private:
  base::FilePath test_data_dir_;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_JAVATESTS_PAINT_PREVIEW_TEST_SERVICE_H_
