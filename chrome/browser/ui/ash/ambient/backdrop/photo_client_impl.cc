// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ambient/backdrop/photo_client_impl.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "base/base64.h"
#include "base/guid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/assistant/internal/proto/google3/backdrop/backdrop.pb.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {

constexpr char kPhotosOAuthScope[] = "https://www.googleapis.com/auth/photos";

constexpr char kProtoMimeType[] = "application/protobuf";

// Max body size in bytes to download.
constexpr int kMaxBodySizeBytes = 1 * 1024 * 1024;  // 1 MiB

std::string GetClientId() {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  DCHECK(prefs);

  std::string client_id =
      prefs->GetString(ash::ambient::prefs::kAmbientBackdropClientId);
  if (client_id.empty()) {
    client_id = base::GenerateGUID();
    prefs->SetString(ash::ambient::prefs::kAmbientBackdropClientId, client_id);
  }

  return client_id;
}

std::unique_ptr<network::ResourceRequest> CreateResourceRequest(
    const chromeos::ambient::BackdropClientConfig::Request& request) {
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

ash::PhotoController::Topic CreateTopicFrom(
    const backdrop::ScreenUpdate::Topic& backdrop_topic) {
  ash::PhotoController::Topic topic;
  topic.url = backdrop_topic.url();
  if (!backdrop_topic.portrait_image_url().empty())
    topic.portrait_image_url = backdrop_topic.portrait_image_url();

  return topic;
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

    loader_factory_ =
        ProfileManager::GetActiveUserProfile()->GetURLLoaderFactory();

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

PhotoClientImpl::PhotoClientImpl() = default;

PhotoClientImpl::~PhotoClientImpl() = default;

void PhotoClientImpl::FetchTopicInfo(OnTopicInfoFetchedCallback callback) {
  // TODO(b/148463064): Access token will be requested and cached before
  // entering lock screen.
  // Consolidate the functions of StartToFetchTopicInfo, StartToGetSettings, and
  // StartToUpdateSettings after this is done.
  RequestAccessToken(base::BindOnce(&PhotoClientImpl::StartToFetchTopicInfo,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(callback)));
}

void PhotoClientImpl::GetSettings(GetSettingsCallback callback) {
  RequestAccessToken(base::BindOnce(&PhotoClientImpl::StartToGetSettings,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(callback)));
}

void PhotoClientImpl::UpdateSettings(int topic_source,
                                     UpdateSettingsCallback callback) {
  RequestAccessToken(base::BindOnce(&PhotoClientImpl::StartToUpdateSettings,
                                    weak_factory_.GetWeakPtr(), topic_source,
                                    std::move(callback)));
}

void PhotoClientImpl::RequestAccessToken(GetAccessTokenCallback callback) {
  auto* profile = ProfileManager::GetActiveUserProfile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo account_info = identity_manager->GetPrimaryAccountInfo(
      signin::ConsentLevel::kNotRequired);

  signin::ScopeSet scopes;
  scopes.insert(kPhotosOAuthScope);
  // TODO(b/148463064): Handle retry refresh token and multiple requests.
  // Currently only one request is allowed.
  DCHECK(!access_token_fetcher_);
  access_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account_info.account_id, /*oauth_consumer_name=*/"ChromeOS_AmbientMode",

      scopes, base::BindOnce(std::move(callback), account_info.gaia),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void PhotoClientImpl::StartToFetchTopicInfo(
    OnTopicInfoFetchedCallback callback,
    const std::string& gaia_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();
  if (gaia_id.empty() || access_token_info.token.empty()) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  std::string client_id = GetClientId();
  chromeos::ambient::BackdropClientConfig::Request request =
      backdrop_client_config_.CreateFetchTopicInfoRequest(
          gaia_id, access_token_info.token, client_id);
  auto resource_request = CreateResourceRequest(request);

  // |base::Unretained| is safe because this instance outlives
  // |backdrop_url_loader_|.
  DCHECK(!backdrop_url_loader_);
  backdrop_url_loader_ = std::make_unique<BackdropURLLoader>();
  backdrop_url_loader_->Start(
      std::move(resource_request), request.body, NO_TRAFFIC_ANNOTATION_YET,
      base::BindOnce(&PhotoClientImpl::OnTopicInfoFetched,
                     base::Unretained(this), std::move(callback)));
}

void PhotoClientImpl::OnTopicInfoFetched(
    OnTopicInfoFetchedCallback callback,
    std::unique_ptr<std::string> response) {
  DCHECK(backdrop_url_loader_);
  backdrop_url_loader_.reset();

  using BackdropClientConfig = chromeos::ambient::BackdropClientConfig;
  backdrop::ScreenUpdate::Topic backdrop_topic =
      BackdropClientConfig::ParseFetchTopicInfoResponse(*response);
  ash::PhotoController::Topic topic = CreateTopicFrom(backdrop_topic);
  std::move(callback).Run(topic);
}

void PhotoClientImpl::StartToGetSettings(
    GetSettingsCallback callback,
    const std::string& gaia_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  if (gaia_id.empty() || access_token_info.token.empty()) {
    std::move(callback).Run(/*topic_source=*/base::nullopt);
    return;
  }

  std::string client_id = GetClientId();
  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateGetSettingsRequest(
          gaia_id, access_token_info.token, client_id);
  auto resource_request = CreateResourceRequest(request);

  // |base::Unretained| is safe because this instance outlives
  // |backdrop_url_loader_|.
  DCHECK(!backdrop_url_loader_);
  backdrop_url_loader_ = std::make_unique<BackdropURLLoader>();
  backdrop_url_loader_->Start(
      std::move(resource_request), request.body, NO_TRAFFIC_ANNOTATION_YET,
      base::BindOnce(&PhotoClientImpl::OnGetSettings, base::Unretained(this),
                     std::move(callback)));
}

void PhotoClientImpl::OnGetSettings(GetSettingsCallback callback,
                                    std::unique_ptr<std::string> response) {
  DCHECK(backdrop_url_loader_);
  backdrop_url_loader_.reset();

  int topic_source = BackdropClientConfig::ParseGetSettingsResponse(*response);
  if (topic_source == -1)
    std::move(callback).Run(base::nullopt);
  else
    std::move(callback).Run(topic_source);
}

void PhotoClientImpl::StartToUpdateSettings(
    int topic_source,
    UpdateSettingsCallback callback,
    const std::string& gaia_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  if (gaia_id.empty() || access_token_info.token.empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::string client_id = GetClientId();
  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateUpdateSettingsRequest(
          gaia_id, access_token_info.token, client_id, topic_source);
  auto resource_request = CreateResourceRequest(request);

  // |base::Unretained| is safe because this instance outlives
  // |backdrop_url_loader_|.
  DCHECK(!backdrop_url_loader_);
  backdrop_url_loader_ = std::make_unique<BackdropURLLoader>();
  backdrop_url_loader_->Start(
      std::move(resource_request), request.body, NO_TRAFFIC_ANNOTATION_YET,
      base::BindOnce(&PhotoClientImpl::OnUpdateSettings, base::Unretained(this),
                     std::move(callback)));
}

void PhotoClientImpl::OnUpdateSettings(UpdateSettingsCallback callback,
                                       std::unique_ptr<std::string> response) {
  DCHECK(backdrop_url_loader_);
  backdrop_url_loader_.reset();

  const bool success =
      BackdropClientConfig::ParseUpdateSettingsResponse(*response);
  std::move(callback).Run(success);
}
