// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/change_password_url_service_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using network::SimpleURLLoader;

constexpr size_t kMaxDownloadSize = 50 * 1024;
constexpr int kMaxRetries = 3;
constexpr base::TimeDelta kFetchTimeout = base::TimeDelta::FromSeconds(3);
}  // namespace

namespace password_manager {

constexpr char ChangePasswordUrlServiceImpl::kChangePasswordUrlOverrideUrl[];

ChangePasswordUrlServiceImpl::ChangePasswordUrlServiceImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service)
    : url_loader_factory_(std::move(url_loader_factory)),
      pref_service_(pref_service) {}

ChangePasswordUrlServiceImpl::~ChangePasswordUrlServiceImpl() = default;

void ChangePasswordUrlServiceImpl::Initialize() {
  if (started_fetching_) {
    return;
  }
  started_fetching_ = true;

  // Don't fetch the gstatic file when PasswordManager policy is disabled.
  if (!pref_service_->GetBoolean(
          password_manager::prefs::kCredentialsEnableService)) {
    fetch_complete_ = true;
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kChangePasswordUrlOverrideUrl);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "gstatic_change_password_override_urls",
          R"(
        semantics {
          sender: "Password Manager"
          description:
            "Downloads a JSON file hosted by gstatic containing a map from "
            "host to change-password url. These urls are used by password "
            "checkup to link to user directly to a password change form when "
            "the password is compromised."
            "Background: when a user has compromised credentials, we want to "
            "link directly to a password change form. Some websites implement "
            "the .well-known/change-password path that points to the site's "
            "password change form. For popular sites we manually looked up the "
            "url and saved them in this JSON file to provide a fallback when "
            ".well-known/change-password is not supported."
            "Spec: https://wicg.github.io/change-password-url/"
          trigger:
            "When the user visits chrome://settings/passwords/check or "
            "[ORIGIN]/.well-known/change-password special URL, Chrome makes "
            "this additional request. This can also be made when a "
            "compromised password dialog appears e.g. after a sign in."
          data:
            "The request body is empty. No user data is included."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "Disabled when the password manager is disabled."
            "disabled."
          chrome_policy {
            PasswordManagerEnabled {
              policy_options {mode: MANDATORY}
              PasswordManagerEnabled: false
            }
          }
        })");
  url_loader_ =
      SimpleURLLoader::Create(std::move(resource_request), traffic_annotation);
  url_loader_->SetRetryOptions(kMaxRetries,
                               SimpleURLLoader::RETRY_ON_5XX |
                                   SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                                   SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);
  url_loader_->SetTimeoutDuration(kFetchTimeout);
  // Binding the callback to |this| is safe, because the navigationthrottle
  // defers if the request is not received yet. Thereby the throttle still exist
  // when the response arrives.
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&ChangePasswordUrlServiceImpl::OnFetchComplete,
                     base::Unretained(this)),
      kMaxDownloadSize);
}

void ChangePasswordUrlServiceImpl::GetChangePasswordUrl(
    const url::Origin& origin,
    UrlCallback callback) {
  DCHECK(started_fetching_) << "Call Initialize() before.";
  if (fetch_complete_) {
    std::move(callback).Run(ChangePasswordUrlFor(origin));
  } else {
    url_callbacks_.emplace_back(origin, std::move(callback));
  }
}

void ChangePasswordUrlServiceImpl::OnFetchComplete(
    std::unique_ptr<std::string> response_body) {
  fetch_complete_ = true;
  // TODO(crbug.com/1086141): Log error codes in histograms.
  if (response_body) {
    base::Optional<base::Value> data = base::JSONReader::Read(*response_body);
    if (data && data->is_dict()) {
      for (auto&& url_pair : data->DictItems()) {
        if (url_pair.second.is_string()) {
          GURL url = GURL(url_pair.second.GetString());
          if (url.is_valid()) {
            change_password_url_map_.try_emplace(change_password_url_map_.end(),
                                                 url_pair.first, url);
          }
        }
      }
    }
  }

  for (auto& url_callback : std::exchange(url_callbacks_, {})) {
    GURL url = ChangePasswordUrlFor(url_callback.first);
    std::move(url_callback.second).Run(std::move(url));
  }
}

GURL ChangePasswordUrlServiceImpl::ChangePasswordUrlFor(
    const url::Origin& origin) {
  std::string domain_and_registry =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  auto it = change_password_url_map_.find(domain_and_registry);
  if (it != change_password_url_map_.end()) {
    return it->second;
  }
  // Fallback if no valid change-password url available or request failed.
  return origin.GetURL();
}

}  // namespace password_manager
