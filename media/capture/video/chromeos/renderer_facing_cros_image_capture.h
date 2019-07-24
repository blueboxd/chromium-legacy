// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_RENDERER_FACING_CROS_IMAGE_CAPTURE_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_RENDERER_FACING_CROS_IMAGE_CAPTURE_H_

#include <string>

#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/mojo/cros_image_capture.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace media {

// Intermediate layer for communicating from renderer to CrosImageCapture
// implementation. It will map the source id recognized by renderer to the
// actual video device id.
class CAPTURE_EXPORT RendererFacingCrosImageCapture
    : public cros::mojom::CrosImageCapture {
 public:
  using WithRealIdCallback =
      base::OnceCallback<void(const base::Optional<std::string>&)>;
  using DeviceIdMappingCallback =
      base::RepeatingCallback<void(const std::string&, WithRealIdCallback)>;
  using IntentCallback = base::RepeatingCallback<
      void(uint32_t, bool, const std::vector<uint8_t>&)>;

  // Create an intermediate layer between renderer to the actual
  // CrosImageCapture implementation. This class should use |api_ptr| to
  // communicate with the actual CrosImageCapture implementation and use
  // |mapping_callback| to map the device id for every calls that inputs device
  // id.
  RendererFacingCrosImageCapture(cros::mojom::CrosImageCapturePtr api_ptr,
                                 DeviceIdMappingCallback mapping_callback,
                                 IntentCallback intent_callback);
  ~RendererFacingCrosImageCapture() override;

  void GetCameraInfoWithRealId(GetCameraInfoCallback callback,
                               const base::Optional<std::string>& device_id);

  void SetReprocessOptionWithRealId(
      cros::mojom::Effect effect,
      SetReprocessOptionCallback callback,
      const base::Optional<std::string>& device_id);

  void SetFpsRangeWithRealId(const uint32_t stream_width,
                             const uint32_t stream_height,
                             const int32_t min_frame_rate,
                             const int32_t max_frame_rate,
                             SetFpsRangeCallback callback,
                             const base::Optional<std::string>& device_id);

  // cros::mojom::CrosImageCapture implementations.
  void GetCameraInfo(const std::string& source_id,
                     GetCameraInfoCallback callback) override;
  void SetReprocessOption(const std::string& source_id,
                          cros::mojom::Effect effect,
                          SetReprocessOptionCallback callback) override;
  void SetFpsRange(const std::string& source_id,
                   const uint32_t stream_width,
                   const uint32_t stream_height,
                   const int32_t min_frame_rate,
                   const int32_t max_frame_rate,
                   SetFpsRangeCallback callback) override;

  void OnIntentHandled(uint32_t intent_id,
                       bool is_success,
                       const std::vector<uint8_t>& captured_data) override;

 private:
  cros::mojom::CrosImageCapturePtr cros_image_capture_;

  DeviceIdMappingCallback mapping_callback_;

  IntentCallback intent_callback_;

  base::WeakPtrFactory<RendererFacingCrosImageCapture> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererFacingCrosImageCapture);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_RENDERER_FACING_CROS_IMAGE_CAPTURE_H_