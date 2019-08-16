// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_COOKIE_MANAGER_H_
#define SERVICES_NETWORK_TEST_TEST_COOKIE_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace network {

// Stubbed out implementation of network::mojom::CookieManager for
// tests.
class TestCookieManager : public network::mojom::CookieManager {
 public:
  TestCookieManager();
  ~TestCookieManager() override;

  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const std::string& source_scheme,
                          const net::CookieOptions& cookie_options,
                          SetCanonicalCookieCallback callback) override;
  void GetAllCookies(GetAllCookiesCallback callback) override {}
  void GetCookieList(const GURL& url,
                     const net::CookieOptions& cookie_options,
                     GetCookieListCallback callback) override {}
  void DeleteCanonicalCookie(const net::CanonicalCookie& cookie,
                             DeleteCanonicalCookieCallback callback) override {}
  void DeleteCookies(network::mojom::CookieDeletionFilterPtr filter,
                     DeleteCookiesCallback callback) override {}
  void AddCookieChangeListener(
      const GURL& url,
      const base::Optional<std::string>& name,
      network::mojom::CookieChangeListenerPtr listener) override;
  void AddGlobalChangeListener(
      network::mojom::CookieChangeListenerPtr notification_pointer) override {}
  void CloneInterface(
      network::mojom::CookieManagerRequest new_interface) override {}
  void FlushCookieStore(FlushCookieStoreCallback callback) override {}
  void AllowFileSchemeCookies(
      bool allow,
      AllowFileSchemeCookiesCallback callback) override {}
  void SetContentSettings(
      const std::vector<::ContentSettingPatternSource>& settings) override {}
  void SetForceKeepSessionState() override {}
  void BlockThirdPartyCookies(bool block) override {}

  void DispatchCookieChange(const net::CanonicalCookie& cookie,
                            network::mojom::CookieChangeCause cause);

 private:
  // List of observers receiving cookie change notifications.
  std::vector<network::mojom::CookieChangeListenerPtr> cookie_change_listeners_;
};

}  // namespace network
#endif  // SERVICES_NETWORK_TEST_TEST_COOKIE_MANAGER_H_
