// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_scripts_fetcher_impl.h"

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
constexpr int kCacheTimeoutInMinutes = 5;
constexpr int kFetchTimeoutInSeconds = 3;

constexpr int kMaxDownloadSizeInBytes = 10 * 1024;
}  // namespace

namespace password_manager {

constexpr char kChangePasswordScriptsListUrl[] =
    "https://www.gstatic.com/chrome/duplex/change_password_scripts.json";

PasswordScriptsFetcherImpl::PasswordScriptsFetcherImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

PasswordScriptsFetcherImpl::~PasswordScriptsFetcherImpl() = default;

void PasswordScriptsFetcherImpl::PrewarmCache() {
  if (IsCacheStale())
    StartFetch();
}

void PasswordScriptsFetcherImpl::GetPasswordScriptAvailability(
    const url::Origin& origin,
    ResponseCallback callback) {
  if (IsCacheStale()) {
    pending_callbacks_.emplace_back(
        std::make_pair(origin, std::move(callback)));
    StartFetch();
    return;
  }

  RunResponseCallback(origin, std::move(callback));
}

void PasswordScriptsFetcherImpl::StartFetch() {
  static const base::NoDestructor<base::TimeDelta> kFetchTimeout(
      base::TimeDelta::FromMinutes(kFetchTimeoutInSeconds));
  if (url_loader_)
    return;
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kChangePasswordScriptsListUrl);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gstatic_change_password_scripts",
                                          R"(
        semantics {
          sender: "Password Manager"
          description:
            "A JSON file hosted by gstatic containing a map of password change"
            "scripts to optional parameters for those scripts."
          trigger:
            "When the user visits chrome://settings/passwords/check or "
            "makes Safety Check in settings or sees a leak warning."
          data:
            "The request body is empty. No user data is included."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "The user can enable or disable automatic password leak checks in "
            "Chrome's security settings. The feature is enabled by default."
        })");
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->SetTimeoutDuration(*kFetchTimeout);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PasswordScriptsFetcherImpl::OnFetchComplete,
                     base::Unretained(this)),
      kMaxDownloadSizeInBytes);
}

void PasswordScriptsFetcherImpl::OnFetchComplete(
    std::unique_ptr<std::string> response_body) {
  url_loader_.reset();
  last_fetch_timestamp_ = base::TimeTicks::Now();
  password_change_domains_.clear();

  if (response_body) {
    base::Optional<base::Value> data = base::JSONReader::Read(*response_body);
    if (data != base::nullopt && data->is_dict()) {
      for (const auto& it : data->DictItems()) {
        // |it.second| is not used at the moment and reserved for
        // domain-specific parameters.
        GURL url(it.first);
        if (url.is_valid()) {
          url::Origin origin = url::Origin::Create(url);
          password_change_domains_.insert(origin);
        }
      }
    }
  }

  for (auto& callback : std::exchange(pending_callbacks_, {}))
    RunResponseCallback(std::move(callback.first), std::move(callback.second));
}

bool PasswordScriptsFetcherImpl::IsCacheStale() const {
  static const base::NoDestructor<base::TimeDelta> kCacheTimeout(
      base::TimeDelta::FromMinutes(kCacheTimeoutInMinutes));
  return last_fetch_timestamp_.is_null() ||
         base::TimeTicks::Now() - last_fetch_timestamp_ >= *kCacheTimeout;
}

void PasswordScriptsFetcherImpl::RunResponseCallback(
    url::Origin origin,
    ResponseCallback callback) {
  DCHECK(!url_loader_);     // Fetching is not running.
  DCHECK(!IsCacheStale());  // Cache is ready.
  bool has_script =
      password_change_domains_.find(origin) != password_change_domains_.end();
  std::move(callback).Run(has_script);
}

}  // namespace password_manager
