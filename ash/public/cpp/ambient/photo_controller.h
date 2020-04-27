// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_PHOTO_CONTROLLER_H_
#define ASH_PUBLIC_CPP_AMBIENT_PHOTO_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// Interface for a class which is responsible for managing photos in the ambient
// mode in ash.
class ASH_PUBLIC_EXPORT PhotoController {
 public:
  // Topic contains the information we need for rendering photo frame for
  // Ambient Mode. Corresponding to the |backdrop::ScreenUpdate::Topic| proto.
  struct Topic {
    Topic();
    ~Topic();
    Topic(const Topic&);
    Topic& operator=(const Topic&);

    // Image url.
    std::string url;

    // Optional for non-cropped portrait style images. The same image as in
    // |url| but it is not cropped, which is better for portrait displaying.
    base::Optional<std::string> portrait_image_url;
  };

  // WeatherInfo contains the weather information we need for rendering a
  // glanceable weather content on Ambient Mode. Corresponding to the
  // |backdrop::WeatherInfo| proto.
  struct WeatherInfo {
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
  struct ScreenUpdate {
    ScreenUpdate();
    ScreenUpdate(const ScreenUpdate&);
    ScreenUpdate& operator=(const ScreenUpdate&);
    ~ScreenUpdate();

    // A list of |Topic| (size >= 0).
    std::vector<Topic> next_topics;

    // Weather information with weather condition icon and temperature in
    // Fahrenheit. Will be a null-opt if:
    // 1. The weather setting was disabled in the request, or
    // 2. Fatal errors, such as response parsing failure, happened during the
    // process, and a dummy |ScreenUpdate| instance was returned to indicate
    // the error.
    base::Optional<WeatherInfo> weather_info;
  };

  static PhotoController* Get();

  // Start fetching next |ScreenUpdate| from the backdrop server. The specified
  // download callback will be run upon completion and returns a null image
  // if: 1. the response did not have the desired fields or urls or, 2. the
  // download attempt from that url failed. The |icon_callback| also returns
  // the weather temperature in Fahrenheit together with the image.
  using PhotoDownloadCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;
  using WeatherIconDownloadCallback =
      base::OnceCallback<void(base::Optional<float>, const gfx::ImageSkia&)>;
  virtual void GetNextScreenUpdateInfo(
      PhotoDownloadCallback photo_callback,
      WeatherIconDownloadCallback icon_callback) = 0;

  using GetSettingsCallback =
      base::OnceCallback<void(const base::Optional<int>& topic_source)>;
  using UpdateSettingsCallback = base::OnceCallback<void(bool success)>;

  // Get settings.
  virtual void GetSettings(GetSettingsCallback callback) = 0;

  // Update settings.
  virtual void UpdateSettings(int topic_source,
                              UpdateSettingsCallback callback) = 0;

 protected:
  PhotoController();
  virtual ~PhotoController();

 private:
  DISALLOW_COPY_AND_ASSIGN(PhotoController);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_PHOTO_CONTROLLER_H_
