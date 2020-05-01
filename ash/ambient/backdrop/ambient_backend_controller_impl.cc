// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/backdrop/ambient_backend_controller_impl.h"

#include <utility>
#include <vector>

#include "ash/ambient/ambient_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/base64.h"
#include "base/guid.h"
#include "base/time/time.h"
#include "chromeos/assistant/internal/proto/google3/backdrop/backdrop.pb.h"
#include "components/prefs/pref_service.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace ash {

namespace {

using BackdropClientConfig = chromeos::ambient::BackdropClientConfig;

constexpr char kProtoMimeType[] = "application/protobuf";

// Max body size in bytes to download.
constexpr int kMaxBodySizeBytes = 1 * 1024 * 1024;  // 1 MiB

std::string GetClientId() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  DCHECK(prefs);

  std::string client_id =
      prefs->GetString(ambient::prefs::kAmbientBackdropClientId);
  if (client_id.empty()) {
    client_id = base::GenerateGUID();
    prefs->SetString(ambient::prefs::kAmbientBackdropClientId, client_id);
  }

  return client_id;
}

std::unique_ptr<network::ResourceRequest> CreateResourceRequest(
    const BackdropClientConfig::Request& request) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(request.url);
  resource_request->method = request.method;
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  for (const auto& header : request.headers) {
    std::string encoded_value;
    if (header.needs_base_64_encoded)
      base::Base64Encode(header.value, &encoded_value);
    else
      encoded_value = header.value;

    resource_request->headers.SetHeader(header.name, encoded_value);
  }

  return resource_request;
}

// Helper function to save the information we got from the backdrop server to a
// public struct so that they can be accessed by public codes.
ash::ScreenUpdate ToScreenUpdate(
    const backdrop::ScreenUpdate& backdrop_screen_update) {
  ash::ScreenUpdate screen_update;
  // Parse |AmbientModeTopic|.
  int topics_size = backdrop_screen_update.next_topics_size();
  if (topics_size > 0) {
    for (auto backdrop_topic : backdrop_screen_update.next_topics()) {
      ash::AmbientModeTopic topic;
      DCHECK(backdrop_topic.has_url());
      topic.url = backdrop_topic.url();
      if (backdrop_topic.has_portrait_image_url())
        topic.portrait_image_url = backdrop_topic.portrait_image_url();
      screen_update.next_topics.emplace_back(topic);
    }
  }

  // Parse |WeatherInfo|.
  if (backdrop_screen_update.has_weather_info()) {
    backdrop::WeatherInfo backdrop_weather_info =
        backdrop_screen_update.weather_info();
    ash::WeatherInfo weather_info;
    if (backdrop_weather_info.has_condition_icon_url())
      weather_info.condition_icon_url =
          backdrop_weather_info.condition_icon_url();
    if (backdrop_weather_info.has_temp_f())
      weather_info.temp_f = backdrop_weather_info.temp_f();
    screen_update.weather_info = weather_info;
  }
  return screen_update;
}

}  // namespace

// Helper class for handling Backdrop service requests.
class BackdropURLLoader {
 public:
  BackdropURLLoader() = default;
  BackdropURLLoader(const BackdropURLLoader&) = delete;
  BackdropURLLoader& operator=(const BackdropURLLoader&) = delete;
  ~BackdropURLLoader() = default;

  // Starts downloading the proto. |request_body| is a serialized proto and
  // will be used as the upload body.
  void Start(std::unique_ptr<network::ResourceRequest> resource_request,
             const std::string& request_body,
             const net::NetworkTrafficAnnotationTag& traffic_annotation,
             network::SimpleURLLoader::BodyAsStringCallback callback) {
    // No ongoing downloading task.
    DCHECK(!simple_loader_);

    loader_factory_ = AmbientClient::Get()->GetURLLoaderFactory();

    // TODO(b/148818448): This will reset previous request without callback
    // called. Handle parallel/sequential requests to server.
    simple_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    simple_loader_->AttachStringForUpload(request_body, kProtoMimeType);
    // |base::Unretained| is safe because this instance outlives
    // |simple_loader_|.
    simple_loader_->DownloadToString(
        loader_factory_.get(),
        base::BindOnce(&BackdropURLLoader::OnUrlDownloaded,
                       base::Unretained(this), std::move(callback)),
        kMaxBodySizeBytes);
  }

 private:
  // Called when the download completes.
  void OnUrlDownloaded(network::SimpleURLLoader::BodyAsStringCallback callback,
                       std::unique_ptr<std::string> response_body) {
    loader_factory_.reset();

    if (simple_loader_->NetError() == net::OK && response_body) {
      simple_loader_.reset();
      std::move(callback).Run(std::move(response_body));
      return;
    }

    int response_code = -1;
    if (simple_loader_->ResponseInfo() &&
        simple_loader_->ResponseInfo()->headers) {
      response_code = simple_loader_->ResponseInfo()->headers->response_code();
    }

    LOG(ERROR) << "Downloading Backdrop proto failed with error code: "
               << response_code << " with network error"
               << simple_loader_->NetError();
    simple_loader_.reset();
    std::move(callback).Run(std::make_unique<std::string>());
    return;
  }

  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
};

AmbientBackendControllerImpl::AmbientBackendControllerImpl() = default;

AmbientBackendControllerImpl::~AmbientBackendControllerImpl() = default;

void AmbientBackendControllerImpl::FetchScreenUpdateInfo(
    OnScreenUpdateInfoFetchedCallback callback) {
  // Consolidate the functions of FetchScreenUpdateInfoInternal,
  // StartToGetSettings, and StartToUpdateSettings after this is done.
  Shell::Get()->ambient_controller()->RequestAccessToken(base::BindOnce(
      &AmbientBackendControllerImpl::FetchScreenUpdateInfoInternal,
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AmbientBackendControllerImpl::GetSettings(GetSettingsCallback callback) {
  Shell::Get()->ambient_controller()->RequestAccessToken(
      base::BindOnce(&AmbientBackendControllerImpl::StartToGetSettings,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AmbientBackendControllerImpl::UpdateSettings(
    AmbientModeTopicSource topic_source,
    UpdateSettingsCallback callback) {
  Shell::Get()->ambient_controller()->RequestAccessToken(base::BindOnce(
      &AmbientBackendControllerImpl::StartToUpdateSettings,
      weak_factory_.GetWeakPtr(), topic_source, std::move(callback)));
}

void AmbientBackendControllerImpl::SetPhotoRefreshInterval(
    base::TimeDelta interval) {
  Shell::Get()
      ->ambient_controller()
      ->ambient_backend_model()
      ->SetPhotoRefreshInterval(interval);
}

void AmbientBackendControllerImpl::FetchScreenUpdateInfoInternal(
    OnScreenUpdateInfoFetchedCallback callback,
    const std::string& gaia_id,
    const std::string& access_token) {
  if (gaia_id.empty() || access_token.empty()) {
    LOG(ERROR) << "Failed to fetch access token";
    // Returns a dummy instance to indicate the failure.
    std::move(callback).Run(ash::ScreenUpdate());
    return;
  }

  std::string client_id = GetClientId();
  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateFetchScreenUpdateRequest(
          gaia_id, access_token, client_id);
  auto resource_request = CreateResourceRequest(request);

  auto backdrop_url_loader = std::make_unique<BackdropURLLoader>();
  auto* loader_ptr = backdrop_url_loader.get();
  loader_ptr->Start(
      std::move(resource_request), request.body, NO_TRAFFIC_ANNOTATION_YET,
      base::BindOnce(&AmbientBackendControllerImpl::OnScreenUpdateInfoFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(backdrop_url_loader)));
}

void AmbientBackendControllerImpl::OnScreenUpdateInfoFetched(
    OnScreenUpdateInfoFetchedCallback callback,
    std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
    std::unique_ptr<std::string> response) {
  DCHECK(backdrop_url_loader);

  // Parse the |ScreenUpdate| out from the response string.
  // Note that the |backdrop_screen_update| can be a dummy instance if the
  // parsing has failed.
  backdrop::ScreenUpdate backdrop_screen_update =
      BackdropClientConfig::ParseScreenUpdateFromResponse(*response);

  // Store the information to a public struct and notify the caller.
  auto screen_update = ToScreenUpdate(backdrop_screen_update);
  std::move(callback).Run(screen_update);
}

void AmbientBackendControllerImpl::StartToGetSettings(
    GetSettingsCallback callback,
    const std::string& gaia_id,
    const std::string& access_token) {
  if (gaia_id.empty() || access_token.empty()) {
    std::move(callback).Run(/*topic_source=*/base::nullopt);
    return;
  }

  std::string client_id = GetClientId();
  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateGetSettingsRequest(gaia_id, access_token,
                                                       client_id);
  auto resource_request = CreateResourceRequest(request);

  auto backdrop_url_loader = std::make_unique<BackdropURLLoader>();
  auto* loader_ptr = backdrop_url_loader.get();
  loader_ptr->Start(
      std::move(resource_request), request.body, NO_TRAFFIC_ANNOTATION_YET,
      base::BindOnce(&AmbientBackendControllerImpl::OnGetSettings,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(backdrop_url_loader)));
}

void AmbientBackendControllerImpl::OnGetSettings(
    GetSettingsCallback callback,
    std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
    std::unique_ptr<std::string> response) {
  DCHECK(backdrop_url_loader);

  int topic_source = BackdropClientConfig::ParseGetSettingsResponse(*response);
  if (topic_source == -1)
    std::move(callback).Run(base::nullopt);
  else
    std::move(callback).Run(static_cast<AmbientModeTopicSource>(topic_source));
}

void AmbientBackendControllerImpl::StartToUpdateSettings(
    AmbientModeTopicSource topic_source,
    UpdateSettingsCallback callback,
    const std::string& gaia_id,
    const std::string& access_token) {
  if (gaia_id.empty() || access_token.empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::string client_id = GetClientId();
  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateUpdateSettingsRequest(
          gaia_id, access_token, client_id, static_cast<int>(topic_source));
  auto resource_request = CreateResourceRequest(request);

  auto backdrop_url_loader = std::make_unique<BackdropURLLoader>();
  auto* loader_ptr = backdrop_url_loader.get();
  loader_ptr->Start(
      std::move(resource_request), request.body, NO_TRAFFIC_ANNOTATION_YET,
      base::BindOnce(&AmbientBackendControllerImpl::OnUpdateSettings,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(backdrop_url_loader)));
}

void AmbientBackendControllerImpl::OnUpdateSettings(
    UpdateSettingsCallback callback,
    std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
    std::unique_ptr<std::string> response) {
  DCHECK(backdrop_url_loader);

  const bool success =
      BackdropClientConfig::ParseUpdateSettingsResponse(*response);
  std::move(callback).Run(success);
}

}  // namespace ash
