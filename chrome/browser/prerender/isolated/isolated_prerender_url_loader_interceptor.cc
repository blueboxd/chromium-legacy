// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_url_loader_interceptor.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_from_string_url_loader.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_params.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_factory.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_subresource_manager.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_url_loader.h"
#include "chrome/browser/prerender/isolated/prefetched_mainframe_response_container.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

namespace {

Profile* ProfileFromFrameTreeNodeID(int frame_tree_node_id) {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents)
    return nullptr;
  return Profile::FromBrowserContext(web_contents->GetBrowserContext());
}

void ReportProbeLatency(int frame_tree_node_id, base::TimeDelta probe_latency) {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents)
    return;

  IsolatedPrerenderTabHelper* tab_helper =
      IsolatedPrerenderTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return;

  tab_helper->NotifyPrefetchProbeLatency(probe_latency);
}

void RecordCookieWaitTime(base::TimeDelta wait_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "IsolatedPrerender.AfterClick.Mainframe.CookieWaitTime", wait_time,
      base::TimeDelta(), base::TimeDelta::FromSeconds(5), 50);
}

void NotifySubresourceManagerOfBadProbe(int frame_tree_node_id,
                                        const GURL& url) {
  Profile* profile = ProfileFromFrameTreeNodeID(frame_tree_node_id);
  if (!profile)
    return;

  IsolatedPrerenderService* service =
      IsolatedPrerenderServiceFactory::GetForProfile(profile);
  if (!service)
    return;

  IsolatedPrerenderSubresourceManager* subresource_manager =
      service->GetSubresourceManagerForURL(url);
  if (!subresource_manager)
    return;

  subresource_manager->NotifyProbeFailed();
}

}  // namespace

IsolatedPrerenderURLLoaderInterceptor::IsolatedPrerenderURLLoaderInterceptor(
    int frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {}

IsolatedPrerenderURLLoaderInterceptor::
    ~IsolatedPrerenderURLLoaderInterceptor() = default;

bool IsolatedPrerenderURLLoaderInterceptor::
    MaybeInterceptNoStatePrefetchNavigation(
        const network::ResourceRequest& tentative_resource_request) {
  Profile* profile = ProfileFromFrameTreeNodeID(frame_tree_node_id_);
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(profile);
  if (!prerender_manager)
    return false;

  if (!prerender_manager->IsWebContentsPrerendering(web_contents, nullptr))
    return false;

  IsolatedPrerenderService* service =
      IsolatedPrerenderServiceFactory::GetForProfile(profile);
  if (!service)
    return false;

  IsolatedPrerenderSubresourceManager* manager =
      service->GetSubresourceManagerForURL(url_);
  if (!manager)
    return false;

  std::unique_ptr<PrefetchedMainframeResponseContainer> mainframe_response =
      manager->TakeMainframeResponse();
  if (!mainframe_response)
    return false;

  InterceptPrefetchedNavigation(tentative_resource_request,
                                std::move(mainframe_response));
  return true;
}

void IsolatedPrerenderURLLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!loader_callback_);
  loader_callback_ = std::move(callback);
  url_ = tentative_resource_request.url;

  // If this method returns true, the navigation has already been intercepted.
  if (MaybeInterceptNoStatePrefetchNavigation(tentative_resource_request))
    return;

  std::unique_ptr<PrefetchedMainframeResponseContainer> prefetch =
      GetPrefetchedResponse(url_);
  if (!prefetch) {
    DoNotInterceptNavigation();
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kIsolatePrerendersMustProbeOrigin)) {
    StartProbe(
        url_.GetOrigin(),
        base::BindOnce(&IsolatedPrerenderURLLoaderInterceptor::
                           EnsureCookiesCopiedAndInterceptPrefetchedNavigation,
                       base::Unretained(this), tentative_resource_request,
                       std::move(prefetch)));
    return;
  }

  EnsureCookiesCopiedAndInterceptPrefetchedNavigation(
      tentative_resource_request, std::move(prefetch));
}

void IsolatedPrerenderURLLoaderInterceptor::
    EnsureCookiesCopiedAndInterceptPrefetchedNavigation(
        const network::ResourceRequest& tentative_resource_request,
        std::unique_ptr<PrefetchedMainframeResponseContainer> prefetch) {
  // The TabHelper needs to copy cookies over to the main profile's cookie jar
  // before we can commit the mainframe so that subresources have the cookies
  // they need before being put on the wire.
  IsolatedPrerenderTabHelper* tab_helper =
      IsolatedPrerenderTabHelper::FromWebContents(
          content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_));
  if (tab_helper && tab_helper->IsWaitingForAfterSRPCookiesCopy()) {
    cookie_copy_start_time_ = base::TimeTicks::Now();
    tab_helper->SetOnAfterSRPCookieCopyCompleteCallback(base::BindOnce(
        &IsolatedPrerenderURLLoaderInterceptor::InterceptPrefetchedNavigation,
        weak_factory_.GetWeakPtr(), tentative_resource_request,
        std::move(prefetch)));
    return;
  }

  // Record that there was no wait time.
  RecordCookieWaitTime(base::TimeDelta());

  // If the cookies were already copied, commit now.
  InterceptPrefetchedNavigation(tentative_resource_request,
                                std::move(prefetch));
}

void IsolatedPrerenderURLLoaderInterceptor::InterceptPrefetchedNavigation(
    const network::ResourceRequest& tentative_resource_request,
    std::unique_ptr<PrefetchedMainframeResponseContainer> prefetch) {
  if (cookie_copy_start_time_) {
    base::TimeDelta wait_time =
        base::TimeTicks::Now() - *cookie_copy_start_time_;
    DCHECK_GT(wait_time, base::TimeDelta());
    RecordCookieWaitTime(wait_time);
  }

  NotifyPrefetchStatusUpdate(
      base::FeatureList::IsEnabled(features::kIsolatePrerendersMustProbeOrigin)
          ? IsolatedPrerenderTabHelper::PrefetchStatus::
                kPrefetchUsedProbeSuccess
          : IsolatedPrerenderTabHelper::PrefetchStatus::kPrefetchUsedNoProbe);

  std::unique_ptr<IsolatedPrerenderFromStringURLLoader> url_loader =
      std::make_unique<IsolatedPrerenderFromStringURLLoader>(
          std::move(prefetch), tentative_resource_request);
  std::move(loader_callback_).Run(url_loader->ServingResponseHandler());
  // url_loader manages its own lifetime once bound to the mojo pipes.
  url_loader.release();
}

void IsolatedPrerenderURLLoaderInterceptor::DoNotInterceptNavigation() {
  std::move(loader_callback_).Run({});
}

void IsolatedPrerenderURLLoaderInterceptor::OnProbeComplete(
    base::OnceClosure on_success_callback,
    bool success) {
  DCHECK(probe_start_time_.has_value());
  ReportProbeLatency(frame_tree_node_id_,
                     base::TimeTicks::Now() - probe_start_time_.value());

  if (success) {
    std::move(on_success_callback).Run();
    return;
  }

  // Notify the SubresourceManager for this url so that subresources should not
  // be loaded from the prefetch cache.
  NotifySubresourceManagerOfBadProbe(frame_tree_node_id_, url_);

  NotifyPrefetchStatusUpdate(
      IsolatedPrerenderTabHelper::PrefetchStatus::kPrefetchNotUsedProbeFailed);
  DoNotInterceptNavigation();
}

void IsolatedPrerenderURLLoaderInterceptor::StartProbe(
    const GURL& url,
    base::OnceClosure on_success_callback) {
  Profile* profile = ProfileFromFrameTreeNodeID(frame_tree_node_id_);
  if (!profile) {
    DoNotInterceptNavigation();
    return;
  }

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

  probe_start_time_ = base::TimeTicks::Now();

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
  origin_prober_->SetOnCompleteCallback(
      base::BindOnce(&IsolatedPrerenderURLLoaderInterceptor::OnProbeComplete,
                     base::Unretained(this), std::move(on_success_callback)));
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

std::unique_ptr<PrefetchedMainframeResponseContainer>
IsolatedPrerenderURLLoaderInterceptor::GetPrefetchedResponse(const GURL& url) {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  if (!web_contents)
    return nullptr;

  IsolatedPrerenderTabHelper* tab_helper =
      IsolatedPrerenderTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return nullptr;

  return tab_helper->TakePrefetchResponse(url);
}

void IsolatedPrerenderURLLoaderInterceptor::NotifyPrefetchStatusUpdate(
    IsolatedPrerenderTabHelper::PrefetchStatus status) const {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  if (!web_contents) {
    return;
  }

  IsolatedPrerenderTabHelper* tab_helper =
      IsolatedPrerenderTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return;

  DCHECK(url_.is_valid());
  tab_helper->OnPrefetchStatusUpdate(url_, status);
}
