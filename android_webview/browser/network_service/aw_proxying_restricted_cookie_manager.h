// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_PROXYING_RESTRICTED_COOKIE_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_PROXYING_RESTRICTED_COOKIE_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "url/gurl.h"

namespace android_webview {

// A RestrictedCookieManager which conditionally proxies to an underlying
// RestrictedCookieManager, first consulting WebView's cookie settings.
class AwProxyingRestrictedCookieManager
    : public network::mojom::RestrictedCookieManager {
 public:
  // Creates a AwProxyingRestrictedCookieManager that lives on IO thread,
  // binding it to handle communications from |request|. The requests will be
  // delegated to |underlying_rcm|. The resulting object will be owned by the
  // pipe corresponding to |request| and will in turn own |underlying_rcm|.
  //
  // Expects to be called on the UI thread.
  static void CreateAndBind(
      network::mojom::RestrictedCookieManagerPtrInfo underlying_rcm,
      bool is_service_worker,
      int process_id,
      int frame_id,
      network::mojom::RestrictedCookieManagerRequest request);

  ~AwProxyingRestrictedCookieManager() override;

  // network::mojom::RestrictedCookieManager interface:
  void GetAllForUrl(const GURL& url,
                    const GURL& site_for_cookies,
                    network::mojom::CookieManagerGetOptionsPtr options,
                    GetAllForUrlCallback callback) override;
  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& url,
                          const GURL& site_for_cookies,
                          SetCanonicalCookieCallback callback) override;
  void AddChangeListener(const GURL& url,
                         const GURL& site_for_cookies,
                         network::mojom::CookieChangeListenerPtr listener,
                         AddChangeListenerCallback callback) override;

  void SetCookieFromString(const GURL& url,
                           const GURL& site_for_cookies,
                           const std::string& cookie,
                           SetCookieFromStringCallback callback) override;

  void GetCookiesString(const GURL& url,
                        const GURL& site_for_cookies,
                        GetCookiesStringCallback callback) override;

  void CookiesEnabledFor(const GURL& url,
                         const GURL& site_for_cookies,
                         CookiesEnabledForCallback callback) override;

  // This one is internal.
  bool AllowCookies(const GURL& url, const GURL& site_for_cookies) const;

 private:
  AwProxyingRestrictedCookieManager(network::mojom::RestrictedCookieManagerPtr
                                        underlying_restricted_cookie_manager,
                                    bool is_service_worker,
                                    int process_id,
                                    int frame_id);

  static void CreateAndBindOnIoThread(
      network::mojom::RestrictedCookieManagerPtrInfo underlying_rcm,
      bool is_service_worker,
      int process_id,
      int frame_id,
      network::mojom::RestrictedCookieManagerRequest request);

  network::mojom::RestrictedCookieManagerPtr
      underlying_restricted_cookie_manager_;
  bool is_service_worker_;
  int process_id_;
  int frame_id_;

  base::WeakPtrFactory<AwProxyingRestrictedCookieManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AwProxyingRestrictedCookieManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_PROXYING_RESTRICTED_COOKIE_MANAGER_H_
