// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/change_password_url_service_impl.h"

#include "base/callback.h"
#include "base/containers/flat_map.h"
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
  // TODO(crbug.com/1086141): call callback if response available, otherwise
  // save callback.
  url_callbacks_.emplace_back(std::make_pair(origin, std::move(callback)));
}

void ChangePasswordUrlServiceImpl::OnFetchComplete(
    std::unique_ptr<std::string> response_body) {
  // TODO(crbug.com/1086141): handle response and convert JSON.
  change_password_url_map_ = {};

  for (auto& url_callback : std::exchange(url_callbacks_, {})) {
    GURL url = ChangePasswordUrlFor(url_callback.first);
    std::move(url_callback.second).Run(std::move(url));
  }
}

GURL ChangePasswordUrlServiceImpl::ChangePasswordUrlFor(
    const url::Origin& origin) {
  // TODO(crbug.com/1086141): lookup url override from map.

  // Fallback if no change-password url available or request failed
  return origin.GetURL();
}

}  // namespace password_manager
