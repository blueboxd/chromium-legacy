// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_BACKEND_CONTROLLER_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_BACKEND_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/optional.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash {

// Enumeration of the topic source, i.e. where the photos come from.
// Values need to stay in sync with the |topicSource_| in ambient_mode_page.js.
// Art gallery is a super set of art related topic sources in Backdrop service.
enum class AmbientModeTopicSource {
  kMinValue = 0,
  kGooglePhotos = kMinValue,
  kArtGallery = 1,
  kMaxValue = kArtGallery
};

// AmbientModeTopic contains the information we need for rendering photo frame
// for Ambient Mode. Corresponding to the |backdrop::ScreenUpdate::Topic| proto.
struct ASH_PUBLIC_EXPORT AmbientModeTopic {
  AmbientModeTopic();
  AmbientModeTopic(const AmbientModeTopic&);
  AmbientModeTopic& operator=(const AmbientModeTopic&);
  ~AmbientModeTopic();

  // Image url.
  std::string url;

  // Optional for non-cropped portrait style images. The same image as in
  // |url| but it is not cropped and is better for portrait displaying.
  base::Optional<std::string> portrait_image_url;
};

// WeatherInfo contains the weather information we need for rendering a
// glanceable weather content on Ambient Mode. Corresponding to the
// |backdrop::WeatherInfo| proto.
struct ASH_PUBLIC_EXPORT WeatherInfo {
  WeatherInfo();
  WeatherInfo(const WeatherInfo&);
  WeatherInfo& operator=(const WeatherInfo&);
  ~WeatherInfo();

  // The url of the weather condition icon image.
  base::Optional<std::string> condition_icon_url;

  // Weather temperature in Fahrenheit.
  base::Optional<float> temp_f;
};

// Trimmed-down version of |backdrop::ScreenUpdate| proto from the backdrop
// server. It contains necessary information we need to render photo frame and
// glancible weather card in Ambient Mode.
struct ASH_PUBLIC_EXPORT ScreenUpdate {
  ScreenUpdate();
  ScreenUpdate(const ScreenUpdate&);
  ScreenUpdate& operator=(const ScreenUpdate&);
  ~ScreenUpdate();

  // A list of |Topic| (size >= 0).
  std::vector<AmbientModeTopic> next_topics;

  // Weather information with weather condition icon and temperature in
  // Fahrenheit. Will be a null-opt if:
  // 1. The weather setting was disabled in the request, or
  // 2. Fatal errors, such as response parsing failure, happened during the
  // process, and a dummy |ScreenUpdate| instance was returned to indicate
  // the error.
  base::Optional<WeatherInfo> weather_info;
};

// Interface to manage ambient mode backend.
class ASH_PUBLIC_EXPORT AmbientBackendController {
 public:
  using OnScreenUpdateInfoFetchedCallback =
      base::OnceCallback<void(const ScreenUpdate&)>;
  using GetSettingsCallback = base::OnceCallback<void(
      base::Optional<AmbientModeTopicSource> topic_source)>;
  using UpdateSettingsCallback = base::OnceCallback<void(bool success)>;

  static AmbientBackendController* Get();

  AmbientBackendController();
  AmbientBackendController(const AmbientBackendController&) = delete;
  AmbientBackendController& operator=(const AmbientBackendController&) = delete;
  virtual ~AmbientBackendController();

  // Sends request to retrieve |ScreenUpdate| from the backdrop server.
  // Upon completion, |callback| is run with the parsed |ScreenUpdate|. If any
  // errors happened during the process, e.g. failed to fetch access token, a
  // dummy instance will be returned.
  virtual void FetchScreenUpdateInfo(
      OnScreenUpdateInfoFetchedCallback callback) = 0;

  // Get ambient mode Settings from server.
  // Currently only return the AmbientModeTopicSource.
  virtual void GetSettings(GetSettingsCallback callback) = 0;

  // Update ambient mode Settings to server.
  // Currently only updating the AmbientModeTopicSource.
  virtual void UpdateSettings(AmbientModeTopicSource topic_source,
                              UpdateSettingsCallback callback) = 0;

  // Set the photo refresh interval in ambient mode.
  virtual void SetPhotoRefreshInterval(base::TimeDelta interval) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_BACKEND_CONTROLLER_H_
