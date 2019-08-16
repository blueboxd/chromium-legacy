// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_HELPER_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_HELPER_IMPL_H_

#include <string>
#include <vector>

#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace media {

class CAPTURE_EXPORT CameraAppHelperImpl : public cros::mojom::CameraAppHelper {
 public:
  using IntentCallback = base::RepeatingCallback<
      void(uint32_t, bool, const std::vector<uint8_t>&)>;

  explicit CameraAppHelperImpl(IntentCallback intent_callback);
  ~CameraAppHelperImpl() override;

  // cros::mojom::CameraAppHelper implementations.
  void OnIntentHandled(uint32_t intent_id,
                       bool is_success,
                       const std::vector<uint8_t>& captured_data) override;

 private:
  IntentCallback intent_callback_;

  DISALLOW_COPY_AND_ASSIGN(CameraAppHelperImpl);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_HELPER_IMPL_H_