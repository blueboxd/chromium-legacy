// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/change_password_url_service_impl.h"

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

ChangePasswordUrlServiceImpl::ChangePasswordUrlServiceImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

ChangePasswordUrlServiceImpl::~ChangePasswordUrlServiceImpl() = default;

void ChangePasswordUrlServiceImpl::Initialize() {
  if (started_fetching_) {
    return;
  }
  started_fetching_ = true;
  // TODO(crbug.com/1086141): make request to gstatic.
  OnFetchComplete(std::make_unique<std::string>("{}"));
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
