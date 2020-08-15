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
#include "services/network/public/cpp/simple_url_loader.h"

class GURL;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
}

namespace password_manager {

class ChangePasswordUrlServiceImpl
    : public password_manager::ChangePasswordUrlService {
 public:
  explicit ChangePasswordUrlServiceImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service);
  ~ChangePasswordUrlServiceImpl() override;

  // ChangePasswordUrlService:
  void PrefetchURLs() override;
  GURL GetChangePasswordUrl(const GURL& url) override;

  static constexpr char kChangePasswordUrlOverrideUrl[] =
      "https://www.gstatic.com/chrome/password-manager/"
      "change_password_urls.json";

 private:
  // Callback for the the request to gstatic.
  void OnFetchComplete(std::unique_ptr<std::string> response_body);

  // Stores if the request is already started to only fetch once.
  bool started_fetching_ = false;
  // True when the gstatic response arrived.
  bool fetch_complete_ = false;
  // Stores the JSON result for the url overrides.
  base::flat_map<std::string, GURL> change_password_url_map_;
  // URL loader object for the gstatic request.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  // SharedURLLoaderFactory for the gstatic request, argument in the
  // constructor.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // We are only fetching the gstatic file if PasswordManager is enabled.
  // We use the PrefService to check if the PasswordManager is enabled.
  PrefService* pref_service_;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CHANGE_PASSWORD_URL_SERVICE_IMPL_H_
