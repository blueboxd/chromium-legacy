// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_PHOTO_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_PHOTO_CONTROLLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// A wrapper class of SimpleURLLoader to download the photo raw data. In the
// test, this will be used to provide fake data.
class ASH_EXPORT AmbientURLLoader {
 public:
  AmbientURLLoader() = default;
  AmbientURLLoader(const AmbientURLLoader&) = delete;
  AmbientURLLoader& operator=(const AmbientURLLoader&) = delete;
  virtual ~AmbientURLLoader() = default;

  // Download data from the given |url|.
  virtual void Download(
      const std::string& url,
      network::SimpleURLLoader::BodyAsStringCallback callback) = 0;
};

// A wrapper class of |data_decoder| to decode the photo raw data. In the test,
// this will be used to provide fake data.
class ASH_EXPORT AmbientImageDecoder {
 public:
  AmbientImageDecoder() = default;
  AmbientImageDecoder(const AmbientImageDecoder&) = delete;
  AmbientImageDecoder& operator=(const AmbientImageDecoder&) = delete;
  virtual ~AmbientImageDecoder() = default;

  // Decode |encoded_bytes| to ImageSkia.
  virtual void Decode(
      const std::vector<uint8_t>& encoded_bytes,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) = 0;
};

// Class to handle photos in ambient mode.
class ASH_EXPORT AmbientPhotoController : public AmbientBackendModelObserver {
 public:
  // Start fetching next |ScreenUpdate| from the backdrop server. The specified
  // download callback will be run upon completion and returns a null image
  // if: 1. the response did not have the desired fields or urls or, 2. the
  // download attempt from that url failed. The |icon_callback| also returns
  // the weather temperature in Fahrenheit together with the image.
  using TopicsDownloadCallback =
      base::OnceCallback<void(const std::vector<AmbientModeTopic>& topics)>;
  using WeatherIconDownloadCallback =
      base::OnceCallback<void(base::Optional<float>, const gfx::ImageSkia&)>;

  using PhotoDownloadCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;

  AmbientPhotoController();
  ~AmbientPhotoController() override;

  // Start/stop updating the screen contents.
  // We need different logics to update photos and weather info because they
  // have different refreshing intervals. Currently we only update weather info
  // one time when entering ambient mode. Photos will be refreshed every
  // |kPhotoRefreshInterval|.
  void StartScreenUpdate();
  void StopScreenUpdate();

  AmbientBackendModel* ambient_backend_model() {
    return &ambient_backend_model_;
  }

  const base::OneShotTimer& photo_refresh_timer_for_testing() const {
    return photo_refresh_timer_;
  }

  // AmbientBackendModelObserver:
  void OnTopicsChanged() override;

 private:
  friend class AmbientAshTestBase;

  void FetchTopics();

  void ScheduleFetchTopics();

  void ScheduleRefreshImage();

  void GetScreenUpdateInfo();

  // Return a topic to download the image.
  const AmbientModeTopic& GetNextTopic();

  void OnScreenUpdateInfoFetched(const ash::ScreenUpdate& screen_update);

  // Try to read photo raw data from disk.
  void TryReadPhotoRawData();

  // If photo raw data is read successfully, call OnPhotoRawDataAvailable() to
  // decode data. Otherwise, download the raw data and save to disk.
  void OnPhotoRawDataRead(const std::string& image_url,
                          std::unique_ptr<std::string> data);

  void OnPhotoRawDataAvailable(const std::string& image_url,
                               bool need_to_save,
                               std::unique_ptr<std::string> response_body);

  void DecodePhotoRawData(std::unique_ptr<std::string> data);

  void OnPhotoDecoded(const gfx::ImageSkia& image);

  void StartDownloadingWeatherConditionIcon(
      const ash::ScreenUpdate& screen_update);

  // Invoked upon completion of the weather icon download, |icon| can be a null
  // image if the download attempt from the url failed.
  void OnWeatherConditionIconDownloaded(base::Optional<float> temp_f,
                                        const gfx::ImageSkia& icon);

  void set_url_loader_for_testing(
      std::unique_ptr<AmbientURLLoader> url_loader) {
    url_loader_ = std::move(url_loader);
  }

  void set_image_decoder_for_testing(
      std::unique_ptr<AmbientImageDecoder> image_decoder) {
    image_decoder_ = std::move(image_decoder);
  }

  AmbientImageDecoder* get_image_decoder_for_testing() {
    return image_decoder_.get();
  }

  AmbientBackendModel ambient_backend_model_;

  // The timer to refresh photos.
  base::OneShotTimer photo_refresh_timer_;

  // The index of a topic to download.
  size_t topic_index_ = 0;

  // Tracking how many batches of topics have been fetched.
  int topics_batch_fetched_ = 0;

  ScopedObserver<AmbientBackendModel, AmbientBackendModelObserver>
      ambient_backend_model_observer_{this};

  base::FilePath root_path_;

  std::unique_ptr<AmbientURLLoader> url_loader_;

  std::unique_ptr<AmbientImageDecoder> image_decoder_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<AmbientPhotoController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AmbientPhotoController);
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_PHOTO_CONTROLLER_H_
