// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CHANGE_PASSWORD_URL_SERVICE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CHANGE_PASSWORD_URL_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "components/password_manager/core/browser/change_password_url_service.h"

class GURL;

namespace url {
class Origin;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace password_manager {

class ChangePasswordUrlServiceImpl
    : public password_manager::ChangePasswordUrlService {
 public:
  explicit ChangePasswordUrlServiceImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~ChangePasswordUrlServiceImpl() override;

  void Initialize() override;
  // When the gstatic response arrives the callback is called with the override
  // url for the given |url|. If no override is there the origin is returned.
  void GetChangePasswordUrl(const url::Origin& origin,
                            UrlCallback callback) override;

 private:
  // Callback for the the request to gstatic.
  void OnFetchComplete(std::unique_ptr<std::string> response_body);
  // Retrieves the url override from the |change_password_url_map_| for a given
  // origin using eTLD+1. The origin is returned when no override is available.
  GURL ChangePasswordUrlFor(const url::Origin& origin);

  // Stores if the request is already started to only fetch once.
  bool started_fetching_ = false;
  // True when the gstatic response arrived.
  bool fetch_complete_ = false;
  // Stores the JSON result for the url overrides.
  base::flat_map<std::string, GURL> change_password_url_map_;
  // Stores the callbacks that are waiting for the request to finish.
  std::vector<std::pair<url::Origin, base::OnceCallback<void(GURL)>>>
      url_callbacks_;
  // SharedURLLoaderFactory for the gstatic request, argument in the
  // constructor.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CHANGE_PASSWORD_URL_SERVICE_IMPL_H_
