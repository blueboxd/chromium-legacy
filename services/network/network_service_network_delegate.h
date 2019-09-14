// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_SERVICE_NETWORK_DELEGATE_H_
#define SERVICES_NETWORK_NETWORK_SERVICE_NETWORK_DELEGATE_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_delegate_impl.h"
#include "services/network/network_context.h"

namespace network {

// TODO(mmenke):  Look into merging this with URLLoader, and removing the
// NetworkDelegate interface.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkServiceNetworkDelegate
    : public net::NetworkDelegateImpl {
 public:
  // |network_context| is guaranteed to outlive this class.
  NetworkServiceNetworkDelegate(
      bool enable_referrers,
      bool validate_referrer_policy_on_initial_request,
      mojom::ProxyErrorClientPtrInfo proxy_error_client_info,
      NetworkContext* network_context);
  ~NetworkServiceNetworkDelegate() override;

  void set_enable_referrers(bool enable_referrers) {
    enable_referrers_ = enable_referrers;
  }

 private:
  // net::NetworkDelegateImpl implementation.
  int OnBeforeURLRequest(net::URLRequest* request,
                         net::CompletionOnceCallback callback,
                         GURL* new_url) override;
  void OnBeforeSendHeaders(net::URLRequest* request,
                           const net::ProxyInfo& proxy_info,
                           const net::ProxyRetryInfoMap& proxy_retry_info,
                           net::HttpRequestHeaders* headers) override;
  int OnBeforeStartTransaction(net::URLRequest* request,
                               net::CompletionOnceCallback callback,
                               net::HttpRequestHeaders* headers) override;
  int OnHeadersReceived(
      net::URLRequest* request,
      net::CompletionOnceCallback callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) override;
  void OnBeforeRedirect(net::URLRequest* request,
                        const GURL& new_location) override;
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnCompleted(net::URLRequest* request,
                   bool started,
                   int net_error) override;
  void OnPACScriptError(int line_number, const base::string16& error) override;
  bool OnCanGetCookies(const net::URLRequest& request,
                       const net::CookieList& cookie_list,
                       bool allowed_from_caller) override;
  bool OnCanSetCookie(const net::URLRequest& request,
                      const net::CanonicalCookie& cookie,
                      net::CookieOptions* options,
                      bool allowed_from_caller) override;
  bool OnForcePrivacyMode(
      const GURL& url,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin) const override;
  bool OnCancelURLRequestWithPolicyViolatingReferrerHeader(
      const net::URLRequest& request,
      const GURL& target_url,
      const GURL& referrer_url) const override;
  bool OnCanQueueReportingReport(const url::Origin& origin) const override;
  void OnCanSendReportingReports(std::set<url::Origin> origins,
                                 base::OnceCallback<void(std::set<url::Origin>)>
                                     result_callback) const override;
  bool OnCanSetReportingClient(const url::Origin& origin,
                               const GURL& endpoint) const override;
  bool OnCanUseReportingClient(const url::Origin& origin,
                               const GURL& endpoint) const override;

  int HandleClearSiteDataHeader(
      net::URLRequest* request,
      net::CompletionOnceCallback callback,
      const net::HttpResponseHeaders* original_response_headers);

  void FinishedClearSiteData(base::WeakPtr<net::URLRequest> request,
                             net::CompletionOnceCallback callback);
  void FinishedCanSendReportingReports(
      base::OnceCallback<void(std::set<url::Origin>)> result_callback,
      const std::vector<url::Origin>& origins);

  void ForwardProxyErrors(int net_error);

  bool enable_referrers_;
  bool validate_referrer_policy_on_initial_request_;
  mojom::ProxyErrorClientPtr proxy_error_client_;
  NetworkContext* network_context_;

  mutable base::WeakPtrFactory<NetworkServiceNetworkDelegate> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceNetworkDelegate);
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_SERVICE_NETWORK_DELEGATE_H_
