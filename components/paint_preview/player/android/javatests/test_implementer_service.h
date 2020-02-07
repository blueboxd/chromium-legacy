// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_JAVATESTS_TEST_IMPLEMENTER_SERVICE_H_
#define COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_JAVATESTS_TEST_IMPLEMENTER_SERVICE_H_

#include "base/files/file_path.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"

namespace paint_preview {

// A simple implementation of PaintPreviewBaseService used in tests.
class TestImplementerService : public PaintPreviewBaseService {
 public:
  TestImplementerService(const base::FilePath& profile_dir,
                         bool is_off_the_record);
  ~TestImplementerService() override;

  TestImplementerService(const TestImplementerService&) = delete;
  TestImplementerService& operator=(const TestImplementerService&) = delete;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_JAVATESTS_TEST_IMPLEMENTER_SERVICE_H_
