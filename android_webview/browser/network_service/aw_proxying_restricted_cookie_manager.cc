// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_proxying_restricted_cookie_manager.h"

#include "android_webview/browser/aw_cookie_access_policy.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace android_webview {

class AwProxyingRestrictedCookieManagerListener
    : public network::mojom::CookieChangeListener {
 public:
  AwProxyingRestrictedCookieManagerListener(
      const GURL& url,
      const GURL& site_for_cookies,
      base::WeakPtr<AwProxyingRestrictedCookieManager>
          aw_restricted_cookie_manager,
      network::mojom::CookieChangeListenerPtr client_listener)
      : url_(url),
        site_for_cookies_(site_for_cookies),
        aw_restricted_cookie_manager_(aw_restricted_cookie_manager),
        client_listener_(std::move(client_listener)) {}

  void OnCookieChange(const net::CanonicalCookie& cookie,
                      network::mojom::CookieChangeCause cause) override {
    if (aw_restricted_cookie_manager_ &&
        aw_restricted_cookie_manager_->AllowCookies(url_, site_for_cookies_))
      client_listener_->OnCookieChange(cookie, cause);
  }

 private:
  const GURL url_;
  const GURL site_for_cookies_;
  base::WeakPtr<AwProxyingRestrictedCookieManager>
      aw_restricted_cookie_manager_;
  network::mojom::CookieChangeListenerPtr client_listener_;
};

// static
void AwProxyingRestrictedCookieManager::CreateAndBind(
    network::mojom::RestrictedCookieManagerPtrInfo underlying_rcm,
    bool is_service_worker,
    int process_id,
    int frame_id,
    network::mojom::RestrictedCookieManagerRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &AwProxyingRestrictedCookieManager::CreateAndBindOnIoThread,
          std::move(underlying_rcm), is_service_worker, process_id, frame_id,
          std::move(request)));
}

AwProxyingRestrictedCookieManager::~AwProxyingRestrictedCookieManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void AwProxyingRestrictedCookieManager::GetAllForUrl(
    const GURL& url,
    const GURL& site_for_cookies,
    network::mojom::CookieManagerGetOptionsPtr options,
    GetAllForUrlCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (AllowCookies(url, site_for_cookies)) {
    underlying_restricted_cookie_manager_->GetAllForUrl(
        url, site_for_cookies, std::move(options), std::move(callback));
  } else {
    std::move(callback).Run(std::vector<net::CanonicalCookie>());
  }
}

void AwProxyingRestrictedCookieManager::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    const GURL& url,
    const GURL& site_for_cookies,
    SetCanonicalCookieCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (AllowCookies(url, site_for_cookies)) {
    underlying_restricted_cookie_manager_->SetCanonicalCookie(
        cookie, url, site_for_cookies, std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

void AwProxyingRestrictedCookieManager::AddChangeListener(
    const GURL& url,
    const GURL& site_for_cookies,
    network::mojom::CookieChangeListenerPtr listener,
    AddChangeListenerCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  network::mojom::CookieChangeListenerPtr proxy_listener_ptr;
  auto proxy_listener =
      std::make_unique<AwProxyingRestrictedCookieManagerListener>(
          url, site_for_cookies, weak_factory_.GetWeakPtr(),
          std::move(listener));

  mojo::MakeStrongBinding(std::move(proxy_listener),
                          mojo::MakeRequest(&proxy_listener_ptr));

  underlying_restricted_cookie_manager_->AddChangeListener(
      url, site_for_cookies, std::move(proxy_listener_ptr),
      std::move(callback));
}

void AwProxyingRestrictedCookieManager::SetCookieFromString(
    const GURL& url,
    const GURL& site_for_cookies,
    const std::string& cookie,
    SetCookieFromStringCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (AllowCookies(url, site_for_cookies)) {
    underlying_restricted_cookie_manager_->SetCookieFromString(
        url, site_for_cookies, cookie, std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AwProxyingRestrictedCookieManager::GetCookiesString(
    const GURL& url,
    const GURL& site_for_cookies,
    GetCookiesStringCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (AllowCookies(url, site_for_cookies)) {
    underlying_restricted_cookie_manager_->GetCookiesString(
        url, site_for_cookies, std::move(callback));
  } else {
    std::move(callback).Run("");
  }
}

void AwProxyingRestrictedCookieManager::CookiesEnabledFor(
    const GURL& url,
    const GURL& site_for_cookies,
    CookiesEnabledForCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(AllowCookies(url, site_for_cookies));
}

AwProxyingRestrictedCookieManager::AwProxyingRestrictedCookieManager(
    network::mojom::RestrictedCookieManagerPtr
        underlying_restricted_cookie_manager,
    bool is_service_worker,
    int process_id,
    int frame_id)
    : underlying_restricted_cookie_manager_(
          std::move(underlying_restricted_cookie_manager)),
      is_service_worker_(is_service_worker),
      process_id_(process_id),
      frame_id_(frame_id),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

// static
void AwProxyingRestrictedCookieManager::CreateAndBindOnIoThread(
    network::mojom::RestrictedCookieManagerPtrInfo underlying_rcm,
    bool is_service_worker,
    int process_id,
    int frame_id,
    network::mojom::RestrictedCookieManagerRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto wrapper = base::WrapUnique(new AwProxyingRestrictedCookieManager(
      network::mojom::RestrictedCookieManagerPtr(std::move(underlying_rcm)),
      is_service_worker, process_id, frame_id));
  mojo::MakeStrongBinding(std::move(wrapper), std::move(request));
}

bool AwProxyingRestrictedCookieManager::AllowCookies(
    const GURL& url,
    const GURL& site_for_cookies) const {
  if (is_service_worker_) {
    // Service worker cookies are always first-party, so only need to check
    // the global toggle.
    return AwCookieAccessPolicy::GetInstance()->GetShouldAcceptCookies();
  } else {
    return AwCookieAccessPolicy::GetInstance()->AllowCookies(
        url, site_for_cookies, process_id_, frame_id_);
  }
}

}  // namespace android_webview
