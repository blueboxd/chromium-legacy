// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_url_loader_interceptor.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_params.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_url_loader.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

namespace {

bool ShouldInterceptRequestForPrerender(
    int frame_tree_node_id,
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context) {
  if (!base::FeatureList::IsEnabled(features::kIsolatePrerenders))
    return false;

  // Lite Mode must be enabled for this feature to be enabled.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  bool ds_enabled = data_reduction_proxy::DataReductionProxySettings::
      IsDataSaverEnabledByUser(profile->IsOffTheRecord(), profile->GetPrefs());
  if (!ds_enabled)
    return false;

  // TODO(crbug.com/1023486): Add other triggering checks.

  // TODO(robertogden): Bail GetStoragePartitionForSite(url) !=
  // GetDefaultStoragePartitionForSite().

  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents)
    return false;

  if (!tentative_resource_request.url.SchemeIs(url::kHttpsScheme))
    return false;

  DCHECK_EQ(web_contents->GetBrowserContext(), browser_context);

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(browser_context);
  if (!prerender_manager)
    return false;

  return prerender_manager->IsWebContentsPrerendering(web_contents, nullptr);
}

Profile* ProfileFromFrameTreeNodeID(int frame_tree_node_id) {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents)
    return nullptr;
  return Profile::FromBrowserContext(web_contents->GetBrowserContext());
}

}  // namespace

IsolatedPrerenderURLLoaderInterceptor::IsolatedPrerenderURLLoaderInterceptor(
    int frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {}

IsolatedPrerenderURLLoaderInterceptor::
    ~IsolatedPrerenderURLLoaderInterceptor() = default;

void IsolatedPrerenderURLLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!loader_callback_);
  loader_callback_ = std::move(callback);

  if (!ShouldInterceptRequestForPrerender(
          frame_tree_node_id_, tentative_resource_request, browser_context)) {
    OnDoNotInterceptRequest();
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kIsolatePrerendersMustProbeOrigin)) {
    StartProbe(tentative_resource_request, browser_context);
    return;
  }

  OnInterceptRequest(tentative_resource_request, browser_context);
}

void IsolatedPrerenderURLLoaderInterceptor::OnInterceptRequest(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context) {
  std::unique_ptr<IsolatedPrerenderURLLoader> url_loader =
      std::make_unique<IsolatedPrerenderURLLoader>(
          tentative_resource_request,
          content::BrowserContext::GetDefaultStoragePartition(browser_context)
              ->GetURLLoaderFactoryForBrowserProcess(),
          frame_tree_node_id_, 0 /* request_id */);
  std::move(loader_callback_).Run(url_loader->ServingResponseHandler());
  // url_loader manages its own lifetime once bound to the mojo pipes.
  url_loader.release();
  return;
}

void IsolatedPrerenderURLLoaderInterceptor::OnDoNotInterceptRequest() {
  std::move(loader_callback_).Run({});
}

void IsolatedPrerenderURLLoaderInterceptor::CallOnProbeCompleteForTesting(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    bool success) {
  OnProbeComplete(tentative_resource_request, browser_context, success);
}

void IsolatedPrerenderURLLoaderInterceptor::OnProbeComplete(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    bool success) {
  if (success) {
    OnInterceptRequest(tentative_resource_request, browser_context);
    return;
  }
  OnDoNotInterceptRequest();
}

void IsolatedPrerenderURLLoaderInterceptor::StartProbe(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context) {
  Profile* profile = ProfileFromFrameTreeNodeID(frame_tree_node_id_);
  if (!profile) {
    OnDoNotInterceptRequest();
    return;
  }

  GURL url = tentative_resource_request.url.GetOrigin();
  DCHECK(url.SchemeIs(url::kHttpsScheme));

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
              "for Lite mode users when the feature is enabled."
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

  origin_prober_ = std::make_unique<AvailabilityProber>(
      this,
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess(),
      profile->GetPrefs(),
      AvailabilityProber::ClientName::kIsolatedPrerenderOriginCheck, url,
      AvailabilityProber::HttpMethod::kHead, net::HttpRequestHeaders(),
      retry_policy, timeout_policy, traffic_annotation,
      0 /* max_cache_entries */,
      base::TimeDelta::FromSeconds(0) /* revalidate_cache_after */);
  // Unretained is safe here because |this| owns |origin_prober_|.
  origin_prober_->SetOnCompleteCallback(base::BindRepeating(
      &IsolatedPrerenderURLLoaderInterceptor::OnProbeComplete,
      base::Unretained(this), tentative_resource_request, browser_context));
  origin_prober_->SendNowIfInactive(false /* send_only_in_foreground */);
}

bool IsolatedPrerenderURLLoaderInterceptor::ShouldSendNextProbe() {
  return true;
}

bool IsolatedPrerenderURLLoaderInterceptor::IsResponseSuccess(
    net::Error net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  // Any response from the origin is good enough, we expect a net error if the
  // site is blocked.
  return net_error == net::OK;
}
