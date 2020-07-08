// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_controller.h"

#include <string>
#include <utility>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/shell.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/hash/sha1.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Topic related numbers.

// The number of requests to fetch topics.
constexpr int kNumberOfRequests = 50;

// The batch size of topics to fetch in one request.
// Magic number 2 is based on experiments that no curation on Google Photos.
constexpr int kTopicsBatchSize = 2;

// The upper bound of delay to the fetch topics. An random value will be
// generated in the range of |kTopicFetchDelayMax|/2 to |kTopicFetchDelayMax|.

// TODO(b/139953713): Change to a correct time interval.
// E.g. it will be max 36 seconds if we want to fetch 50 batches in 30 mins.
constexpr base::TimeDelta kTopicFetchDelayMax = base::TimeDelta::FromSeconds(3);

constexpr int kMaxImageSizeInBytes = 5 * 1024 * 1024;

constexpr int kMaxReservedAvailableDiskSpaceByte = 200 * 1024 * 1024;

constexpr char kPhotoFileExt[] = ".img";

using DownloadCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;

void DownloadImageFromUrl(const std::string& url, DownloadCallback callback) {
  DCHECK(!url.empty());

  ImageDownloader::Get()->Download(GURL(url), NO_TRAFFIC_ANNOTATION_YET,
                                   base::BindOnce(std::move(callback)));
}

// Get the root path for ambient mode.
base::FilePath GetRootPath() {
  base::FilePath home_dir;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir));
  return home_dir.Append(FILE_PATH_LITERAL(kAmbientModeDirectoryName));
}

void DeletePathRecursively(const base::FilePath& path) {
  base::DeletePathRecursively(path);
}

std::string ToPhotoFileName(const std::string& url) {
  return base::SHA1HashString(url) + std::string(kPhotoFileExt);
}

void ToImageSkia(DownloadCallback callback, const SkBitmap& image) {
  if (image.isNull()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(image);
  image_skia.MakeThreadSafe();

  std::move(callback).Run(image_skia);
}

base::TaskTraits GetTaskTraits() {
  return {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
          base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
}

// TODO: Move to ambient_util.
void WriteFile(const base::FilePath& path, const std::string& data) {
  if (!base::PathExists(GetRootPath()) &&
      !base::CreateDirectory(GetRootPath())) {
    LOG(ERROR) << "Cannot create ambient mode directory.";
    return;
  }

  if (base::SysInfo::AmountOfFreeDiskSpace(GetRootPath()) <
      kMaxReservedAvailableDiskSpaceByte) {
    LOG(WARNING) << "Not enough disk space left.";
    return;
  }

  if (!base::PathExists(path.DirName()) &&
      !base::CreateDirectory(path.DirName())) {
    LOG(ERROR) << "Cannot create a new session directory.";
    return;
  }

  // Create a temp file.
  base::FilePath temp_file;
  if (!base::CreateTemporaryFileInDir(path.DirName(), &temp_file)) {
    LOG(ERROR) << "Cannot create a temporary file.";
    return;
  }

  // Write to the tmp file.
  const int size = data.size();
  int written_size = base::WriteFile(temp_file, data.data(), size);
  if (written_size != size) {
    LOG(ERROR) << "Cannot write the temporary file.";
    base::DeleteFile(temp_file);
    return;
  }

  // Replace the current file with the temp file.
  if (!base::ReplaceFile(temp_file, path, /*error=*/nullptr))
    LOG(ERROR) << "Cannot replace the temporary file.";
}

}  // namespace

class AmbientURLLoaderImpl : public AmbientURLLoader {
 public:
  AmbientURLLoaderImpl() = default;
  ~AmbientURLLoaderImpl() override = default;

  // AmbientURLLoader:
  void Download(
      const std::string& url,
      network::SimpleURLLoader::BodyAsStringCallback callback) override {
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = GURL(url);
    resource_request->method = "GET";
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

    auto simple_loader = network::SimpleURLLoader::Create(
        std::move(resource_request), NO_TRAFFIC_ANNOTATION_YET);
    auto* loader_ptr = simple_loader.get();
    auto loader_factory = AmbientClient::Get()->GetURLLoaderFactory();
    loader_ptr->DownloadToString(
        loader_factory.get(),
        base::BindOnce(&AmbientURLLoaderImpl::OnUrlDownloaded,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       std::move(simple_loader), loader_factory),
        kMaxImageSizeInBytes);
  }

 private:
  // Called when the download completes.
  void OnUrlDownloaded(
      network::SimpleURLLoader::BodyAsStringCallback callback,
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      std::unique_ptr<std::string> response_body) {
    if (simple_loader->NetError() == net::OK && response_body) {
      std::move(callback).Run(std::move(response_body));
      return;
    }

    int response_code = -1;
    if (simple_loader->ResponseInfo() &&
        simple_loader->ResponseInfo()->headers) {
      response_code = simple_loader->ResponseInfo()->headers->response_code();
    }

    LOG(ERROR) << "Downloading Backdrop proto failed with error code: "
               << response_code << " with network error"
               << simple_loader->NetError();
    std::move(callback).Run(std::make_unique<std::string>());
  }

  base::WeakPtrFactory<AmbientURLLoaderImpl> weak_factory_{this};
};

class AmbientImageDecoderImpl : public AmbientImageDecoder {
 public:
  AmbientImageDecoderImpl() = default;
  ~AmbientImageDecoderImpl() override = default;

  // AmbientImageDecoder:
  void Decode(
      const std::vector<uint8_t>& encoded_bytes,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) override {
    data_decoder::DecodeImageIsolated(
        std::move(encoded_bytes), data_decoder::mojom::ImageCodec::DEFAULT,
        /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes,
        /*desired_image_frame_size=*/gfx::Size(),
        base::BindOnce(&ToImageSkia, std::move(callback)));
  }
};

AmbientPhotoController::AmbientPhotoController()
    : url_loader_(std::make_unique<AmbientURLLoaderImpl>()),
      image_decoder_(std::make_unique<AmbientImageDecoderImpl>()),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(GetTaskTraits())) {
  ambient_backend_model_observer_.Add(&ambient_backend_model_);
}

AmbientPhotoController::~AmbientPhotoController() = default;

void AmbientPhotoController::StartScreenUpdate() {
  root_path_ = GetRootPath().Append(FILE_PATH_LITERAL(base::GenerateGUID()));
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&DeletePathRecursively, GetRootPath()),
      base::BindOnce(&AmbientPhotoController::FetchTopics,
                     weak_factory_.GetWeakPtr()));
}

void AmbientPhotoController::StopScreenUpdate() {
  photo_refresh_timer_.Stop();
  topic_index_ = 0;
  topics_batch_fetched_ = 0;
  ambient_backend_model_.Clear();
  weak_factory_.InvalidateWeakPtrs();

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DeletePathRecursively, root_path_));
}

void AmbientPhotoController::OnTopicsChanged() {
  ++topics_batch_fetched_;
  if (topics_batch_fetched_ < kNumberOfRequests)
    ScheduleFetchTopics();

  // The first OnTopicsChanged event triggers the photo refresh.
  if (topics_batch_fetched_ == 1)
    ScheduleRefreshImage();
}

void AmbientPhotoController::FetchTopics() {
  Shell::Get()
      ->ambient_controller()
      ->ambient_backend_controller()
      ->FetchScreenUpdateInfo(
          kTopicsBatchSize,
          base::BindOnce(&AmbientPhotoController::OnScreenUpdateInfoFetched,
                         weak_factory_.GetWeakPtr()));
}

void AmbientPhotoController::ScheduleFetchTopics() {
  const base::TimeDelta kDelay =
      (base::RandDouble() * kTopicFetchDelayMax) / 2 + kTopicFetchDelayMax / 2;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AmbientPhotoController::FetchTopics,
                     weak_factory_.GetWeakPtr()),
      kDelay);
}

void AmbientPhotoController::ScheduleRefreshImage() {
  base::TimeDelta refresh_interval;
  if (!ambient_backend_model_.ShouldFetchImmediately())
    refresh_interval = kPhotoRefreshInterval;

  // |photo_refresh_timer_| will start immediately if ShouldFetchImmediately()
  // is true.
  photo_refresh_timer_.Start(
      FROM_HERE, refresh_interval,
      base::BindOnce(&AmbientPhotoController::TryReadPhotoRawData,
                     weak_factory_.GetWeakPtr()));
}

const AmbientModeTopic& AmbientPhotoController::GetNextTopic() {
  const auto& topics = ambient_backend_model_.topics();
  DCHECK(!topics.empty());

  // We prefetch the first two photos, which will increase the |topic_index_| to
  // 2 in the first batch with size of 2. Then it will reset to 0 if we put this
  // block after the increment of |topic_index_|.
  if (topic_index_ == topics.size())
    topic_index_ = 0;

  return topics[topic_index_++];
}

void AmbientPhotoController::OnScreenUpdateInfoFetched(
    const ash::ScreenUpdate& screen_update) {
  // It is possible that |screen_update| is an empty instance if fatal errors
  // happened during the fetch.
  // TODO(b/148485116): Implement retry logic.
  if (screen_update.next_topics.empty() &&
      !screen_update.weather_info.has_value()) {
    LOG(ERROR) << "The screen update info fetch has failed.";
    return;
  }

  ambient_backend_model_.AppendTopics(screen_update.next_topics);
  StartDownloadingWeatherConditionIcon(screen_update);
}

void AmbientPhotoController::TryReadPhotoRawData() {
  const AmbientModeTopic& topic = GetNextTopic();
  const std::string& image_url = topic.portrait_image_url.value_or(topic.url);

  base::FilePath path = root_path_.Append(ToPhotoFileName(image_url));
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& path) {
            auto data = std::make_unique<std::string>();
            if (!base::ReadFileToString(path, data.get()))
              data = nullptr;
            return data;
          },
          path),
      base::BindOnce(&AmbientPhotoController::OnPhotoRawDataRead,
                     weak_factory_.GetWeakPtr(), image_url));
}

void AmbientPhotoController::OnPhotoRawDataRead(
    const std::string& image_url,
    std::unique_ptr<std::string> data) {
  if (!data || data->empty()) {
    url_loader_->Download(
        image_url,
        base::BindOnce(&AmbientPhotoController::OnPhotoRawDataAvailable,
                       weak_factory_.GetWeakPtr(), image_url,
                       /*need_to_save=*/true));
  } else {
    OnPhotoRawDataAvailable(image_url, /*need_to_save=*/false, std::move(data));
  }
}

void AmbientPhotoController::OnPhotoRawDataAvailable(
    const std::string& image_url,
    bool need_to_save,
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    LOG(ERROR) << "Failed to download image";

    // Continue to get next photo on error.
    // TODO(b/148485116): Add exponential backoff retry logic.
    const base::TimeDelta kDelay = base::TimeDelta::FromMilliseconds(100);
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AmbientPhotoController::ScheduleRefreshImage,
                       weak_factory_.GetWeakPtr()),
        kDelay);
    return;
  }

  const base::FilePath path = root_path_.Append(ToPhotoFileName(image_url));
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& path, bool need_to_save,
             const std::string& data) {
            if (need_to_save)
              WriteFile(path, data);
          },
          path, need_to_save, *response_body),
      base::BindOnce(&AmbientPhotoController::DecodePhotoRawData,
                     weak_factory_.GetWeakPtr(), std::move(response_body)));
}

void AmbientPhotoController::DecodePhotoRawData(
    std::unique_ptr<std::string> data) {
  std::vector<uint8_t> image_bytes(data->begin(), data->end());
  image_decoder_->Decode(image_bytes,
                         base::BindOnce(&AmbientPhotoController::OnPhotoDecoded,
                                        weak_factory_.GetWeakPtr()));
}

void AmbientPhotoController::OnPhotoDecoded(const gfx::ImageSkia& image) {
  base::TimeDelta kDelay;
  if (image.isNull()) {
    LOG(WARNING) << "Image is null";
    kDelay = base::TimeDelta::FromMilliseconds(100);
  } else {
    ambient_backend_model_.AddNextImage(image);
  }

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AmbientPhotoController::ScheduleRefreshImage,
                     weak_factory_.GetWeakPtr()),
      kDelay);
}

void AmbientPhotoController::StartDownloadingWeatherConditionIcon(
    const ash::ScreenUpdate& screen_update) {
  if (!screen_update.weather_info.has_value()) {
    LOG(WARNING) << "No weather info included in the response.";
    return;
  }

  // Ideally we should avoid downloading from the same url again to reduce the
  // overhead, as it's unlikely that the weather condition is changing
  // frequently during the day.
  // TODO(meilinw): avoid repeated downloading by caching the last N url hashes,
  // where N should depend on the icon image size.
  const std::string& icon_url =
      screen_update.weather_info->condition_icon_url.value_or(std::string());
  if (icon_url.empty()) {
    LOG(ERROR) << "No value found for condition icon url in the weather info "
                  "response.";
    return;
  }

  DownloadImageFromUrl(
      icon_url,
      base::BindOnce(&AmbientPhotoController::OnWeatherConditionIconDownloaded,
                     weak_factory_.GetWeakPtr(),
                     screen_update.weather_info->temp_f));
}

void AmbientPhotoController::OnWeatherConditionIconDownloaded(
    base::Optional<float> temp_f,
    const gfx::ImageSkia& icon) {
  // For now we only show the weather card when both fields have values.
  // TODO(meilinw): optimize the behavior with more specific error handling.
  if (icon.isNull() || !temp_f.has_value())
    return;

  ambient_backend_model_.UpdateWeatherInfo(icon, temp_f.value());
}

}  // namespace ash
