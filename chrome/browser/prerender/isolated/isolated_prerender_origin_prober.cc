// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_origin_prober.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/availability/availability_prober.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_params.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/host_port_pair.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/origin.h"

namespace {

class DNSProber : public network::mojom::ResolveHostClient {
 public:
  explicit DNSProber(
      IsolatedPrerenderOriginProber::OnProbeResultCallback callback)
      : callback_(std::move(callback)) {
    DCHECK(callback_);
  }

  ~DNSProber() override {
    if (callback_) {
      // Indicates some kind of mojo error. Play it safe and return no success.
      std::move(callback_).Run(false);
    }
  }

  // network::mojom::ResolveHostClient:
  void OnTextResults(const std::vector<std::string>&) override {}
  void OnHostnameResults(const std::vector<net::HostPortPair>&) override {}
  void OnComplete(
      int32_t error,
      const net::ResolveErrorInfo& resolve_error_info,
      const base::Optional<net::AddressList>& resolved_addresses) override {
    if (callback_) {
      std::move(callback_).Run(error == net::OK);
    }
  }

 private:
  IsolatedPrerenderOriginProber::OnProbeResultCallback callback_;
};

void HTTPProbeHelper(
    std::unique_ptr<AvailabilityProber> prober,
    IsolatedPrerenderOriginProber::OnProbeResultCallback callback,
    bool success) {
  std::move(callback).Run(success);
}

class CanaryCheckDelegate : public AvailabilityProber::Delegate {
 public:
  CanaryCheckDelegate() = default;
  ~CanaryCheckDelegate() = default;

  bool ShouldSendNextProbe() override { return true; }

  bool IsResponseSuccess(net::Error net_error,
                         const network::mojom::URLResponseHead* head,
                         std::unique_ptr<std::string> body) override {
    return net_error == net::OK && head && head->headers &&
           head->headers->response_code() == 200 && body && *body == "OK";
  }
};

class OriginProbeDelegate : public AvailabilityProber::Delegate {
 public:
  OriginProbeDelegate() = default;
  ~OriginProbeDelegate() = default;

  bool ShouldSendNextProbe() override { return true; }

  bool IsResponseSuccess(net::Error net_error,
                         const network::mojom::URLResponseHead* head,
                         std::unique_ptr<std::string> body) override {
    return net_error == net::OK;
  }
};

CanaryCheckDelegate* GetCanaryCheckDelegate() {
  static base::NoDestructor<CanaryCheckDelegate> delegate;
  return delegate.get();
}

OriginProbeDelegate* GetOriginProbeDelegate() {
  static base::NoDestructor<OriginProbeDelegate> delegate;
  return delegate.get();
}

}  // namespace

IsolatedPrerenderOriginProber::IsolatedPrerenderOriginProber(Profile* profile)
    : profile_(profile) {
  if (!IsolatedPrerenderProbingEnabled())
    return;
  if (!IsolatedPrerenderCanaryCheckEnabled())
    return;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("isolated_prerender_canary_check", R"(
          semantics {
            sender: "Isolated Prerender Canary Checker"
            description:
              "Sends a request over HTTP to a known host in order to determine "
              "if the network is subject to web filtering. If this request is "
              "blocked, the Isolated Prerender feature will check that a "
              "navigated site is not blocked by the network before using "
              "proxied resources."
            trigger:
              "Used at browser startup for Lite mode users when the feature is "
              "enabled."
            data: "None."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can control Lite mode on Android via the settings menu. "
              "Lite mode is not available on iOS, and on desktop only for "
              "developer testing."
            policy_exception_justification: "Not implemented."
        })");

  AvailabilityProber::TimeoutPolicy timeout_policy;
  timeout_policy.base_timeout = IsolatedPrerenderProbeTimeout();
  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 0;

  canary_check_ = std::make_unique<AvailabilityProber>(
      GetCanaryCheckDelegate(),
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess(),
      profile_->GetPrefs(),
      AvailabilityProber::ClientName::kIsolatedPrerenderCanaryCheck,
      IsolatedPrerenderCanaryCheckURL(), AvailabilityProber::HttpMethod::kGet,
      net::HttpRequestHeaders(), retry_policy, timeout_policy,
      traffic_annotation, 10 /* max_cache_entries */,
      IsolatedPrerenderCanaryCheckCacheLifetime());

  // If there is no previously cached result for this network then one should be
  // started. If the previous result is stale, the prober will start a probe
  // during |LastProbeWasSuccessful|.
  if (!canary_check_->LastProbeWasSuccessful().has_value()) {
    canary_check_->SendNowIfInactive(false);
  }
}

IsolatedPrerenderOriginProber::~IsolatedPrerenderOriginProber() = default;

bool IsolatedPrerenderOriginProber::ShouldProbeOrigins() {
  if (!IsolatedPrerenderProbingEnabled()) {
    return false;
  }
  if (!IsolatedPrerenderCanaryCheckEnabled()) {
    return true;
  }
  DCHECK(canary_check_);

  base::Optional<bool> result = canary_check_->LastProbeWasSuccessful();
  if (!result.has_value()) {
    // The canary check hasn't completed yet, so this request must probe.
    return true;
  }

  // If the canary check was successful, no probing is needed.
  return !result.value();
}

void IsolatedPrerenderOriginProber::
    SetProbeURLOverrideDelegateOverrideForTesting(
        ProbeURLOverrideDelegate* delegate) {
  override_delegate_ = delegate;
}

bool IsolatedPrerenderOriginProber::IsCanaryCheckCompleteForTesting() const {
  return canary_check_ && canary_check_->LastProbeWasSuccessful().has_value();
}

void IsolatedPrerenderOriginProber::Probe(const GURL& url,
                                          OnProbeResultCallback callback) {
  DCHECK(ShouldProbeOrigins());

  GURL probe_url = url;
  if (override_delegate_) {
    probe_url = override_delegate_->OverrideProbeURL(probe_url);
  }

  switch (IsolatedPrerenderOriginProbeMechanism()) {
    case IsolatedPrerenderOriginProbeType::kDns:
      DNSProbe(probe_url, std::move(callback));
      return;
    case IsolatedPrerenderOriginProbeType::kHttpHead:
      HTTPProbe(probe_url, std::move(callback));
      return;
  }
}

void IsolatedPrerenderOriginProber::DNSProbe(const GURL& url,
                                             OnProbeResultCallback callback) {
  net::NetworkIsolationKey nik =
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url))
          .network_isolation_key();

  network::mojom::ResolveHostParametersPtr resolve_host_parameters =
      network::mojom::ResolveHostParameters::New();
  // This action is navigation-blocking, so use the highest priority.
  resolve_host_parameters->initial_priority = net::RequestPriority::HIGHEST;

  mojo::PendingRemote<network::mojom::ResolveHostClient> client_remote;
  mojo::MakeSelfOwnedReceiver(std::make_unique<DNSProber>(std::move(callback)),
                              client_remote.InitWithNewPipeAndPassReceiver());

  content::BrowserContext::GetDefaultStoragePartition(profile_)
      ->GetNetworkContext()
      ->ResolveHost(net::HostPortPair::FromURL(url), nik,
                    std::move(resolve_host_parameters),
                    std::move(client_remote));
}

void IsolatedPrerenderOriginProber::HTTPProbe(const GURL& url,
                                              OnProbeResultCallback callback) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("isolated_prerender_probe", R"(
          semantics {
            sender: "Isolated Prerender Probe Loader"
            description:
              "Verifies the end to end connection between Chrome and the "
              "origin site that the user is currently navigating to. This is "
              "done during a navigation that was previously prerendered over a "
              "proxy to check that the site is not blocked by middleboxes. "
              "Such prerenders will be used to prefetch render-blocking "
              "content before being navigated by the user without impacting "
              "privacy."
            trigger:
              "Used for sites off of Google SRPs (Search Result Pages) only "
              "for Lite mode users when the experimental feature flag is "
              "enabled."
            data: "None."
            destination: WEBSITE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can control Lite mode on Android via the settings menu. "
              "Lite mode is not available on iOS, and on desktop only for "
              "developer testing."
            policy_exception_justification: "Not implemented."
        })");

  AvailabilityProber::TimeoutPolicy timeout_policy;
  timeout_policy.base_timeout = IsolatedPrerenderProbeTimeout();
  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 0;

  std::unique_ptr<AvailabilityProber> prober =
      std::make_unique<AvailabilityProber>(
          GetOriginProbeDelegate(),
          content::BrowserContext::GetDefaultStoragePartition(profile_)
              ->GetURLLoaderFactoryForBrowserProcess(),
          nullptr /* pref_service */,
          AvailabilityProber::ClientName::kIsolatedPrerenderOriginCheck, url,
          AvailabilityProber::HttpMethod::kHead, net::HttpRequestHeaders(),
          retry_policy, timeout_policy, traffic_annotation,
          0 /* max_cache_entries */,
          base::TimeDelta::FromSeconds(0) /* revalidate_cache_after */);
  AvailabilityProber* prober_ptr = prober.get();

  // Transfer ownership of the prober to the callback so that the class instance
  // is automatically destroyed when the probe is complete.
  OnProbeResultCallback owning_callback =
      base::BindOnce(&HTTPProbeHelper, std::move(prober), std::move(callback));
  prober_ptr->SetOnCompleteCallback(base::BindOnce(std::move(owning_callback)));

  prober_ptr->SendNowIfInactive(false /* send_only_in_foreground */);
}
