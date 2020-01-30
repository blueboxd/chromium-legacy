// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_tab_helper.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/google/core/common/google_util.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

IsolatedPrerenderTabHelper::IsolatedPrerenderTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());
  url_loader_factory_ =
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess();

  NavigationPredictorKeyedService* navigation_predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile_);
  if (navigation_predictor_service) {
    navigation_predictor_service->AddObserver(this);
  }
}

IsolatedPrerenderTabHelper::~IsolatedPrerenderTabHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NavigationPredictorKeyedService* navigation_predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile_);
  if (navigation_predictor_service) {
    navigation_predictor_service->RemoveObserver(this);
  }
}

void IsolatedPrerenderTabHelper::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
}

void IsolatedPrerenderTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  // User is navigating, don't bother prefetching further.
  url_loader_.reset();
}

void IsolatedPrerenderTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }
  if (navigation_handle->IsSameDocument()) {
    return;
  }
  if (!navigation_handle->HasCommitted()) {
    return;
  }

  DCHECK(!PrefetchingActive());
  urls_to_prefetch_.clear();
  prefetched_responses_.clear();
}

std::unique_ptr<PrefetchedResponseContainer>
IsolatedPrerenderTabHelper::TakePrefetchResponse(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = prefetched_responses_.find(url);
  if (it == prefetched_responses_.end())
    return nullptr;

  std::unique_ptr<PrefetchedResponseContainer> response = std::move(it->second);
  prefetched_responses_.erase(it);
  return response;
}

bool IsolatedPrerenderTabHelper::PrefetchingActive() const {
  return !!url_loader_;
}

void IsolatedPrerenderTabHelper::Prefetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly));

  url_loader_.reset();
  if (urls_to_prefetch_.empty())
    return;

  GURL url = urls_to_prefetch_[0];
  urls_to_prefetch_.erase(urls_to_prefetch_.begin());

  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->method = "GET";
  request->load_flags = net::LOAD_DISABLE_CACHE | net::LOAD_PREFETCH;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->headers.SetHeader(content::kCorsExemptPurposeHeaderName, "prefetch");

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("navigation_predictor_srp_prefetch",
                                          R"(
          semantics {
            sender: "Navigation Predictor SRP Prefetch Loader"
            description:
              "Prefetches the mainframe HTML of a page linked from a Google "
              "Search Result Page (SRP). This is done out-of-band of normal "
              "prefetches to allow total isolation of this request from the "
              "rest of browser traffic and user state like cookies and cache."
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

  // TODO(crbug/1023485): Disallow auth challenges.

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);

  // base::Unretained is safe because |url_loader_| is owned by |this|.
  url_loader_->SetOnRedirectCallback(base::BindRepeating(
      &IsolatedPrerenderTabHelper::OnPrefetchRedirect, base::Unretained(this)));

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&IsolatedPrerenderTabHelper::OnPrefetchComplete,
                     base::Unretained(this), url),
      1024 * 1024 * 5 /* 5MB */);
}

void IsolatedPrerenderTabHelper::OnPrefetchRedirect(
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  // TODO(crbug/1023485): Support redirects.
  // Redirects are currently not supported. Calling |Prefetch| will reset the
  // current url loader and maybe start a new one.
  DCHECK(PrefetchingActive());
  Prefetch();
}

void IsolatedPrerenderTabHelper::OnPrefetchComplete(
    const GURL& url,
    std::unique_ptr<std::string> body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(PrefetchingActive());

  if (url_loader_->NetError() == net::OK && body &&
      url_loader_->ResponseInfo()) {
    network::mojom::URLResponseHeadPtr head =
        url_loader_->ResponseInfo()->Clone();
    HandlePrefetchResponse(url, std::move(head), std::move(body));
  }
  Prefetch();
}

void IsolatedPrerenderTabHelper::HandlePrefetchResponse(
    const GURL& url,
    network::mojom::URLResponseHeadPtr head,
    std::unique_ptr<std::string> body) {
  DCHECK(!head->was_fetched_via_cache);
  DCHECK(PrefetchingActive());

  int response_code = head->headers->response_code();
  if (response_code < 200 || response_code >= 300) {
    return;
  }

  if (head->mime_type != "text/html") {
    return;
  }
  std::unique_ptr<PrefetchedResponseContainer> response =
      std::make_unique<PrefetchedResponseContainer>(std::move(head),
                                                    std::move(body));
  prefetched_responses_.emplace(url, std::move(response));
}

void IsolatedPrerenderTabHelper::OnPredictionUpdated(
    const base::Optional<NavigationPredictorKeyedService::Prediction>&
        prediction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(
          features::kPrefetchSRPNavigationPredictions_HTMLOnly)) {
    return;
  }

  // DataSaver must be enabled by the user to use this feature.
  if (!data_reduction_proxy::DataReductionProxySettings::
          IsDataSaverEnabledByUser(profile_->IsOffTheRecord(),
                                   profile_->GetPrefs())) {
    return;
  }

  if (!prediction.has_value()) {
    return;
  }

  if (prediction.value().web_contents() != web_contents()) {
    // We only care about predictions in this tab.
    return;
  }

  if (!google_util::IsGoogleSearchUrl(
          prediction.value().source_document_url())) {
    return;
  }

  for (const GURL& url : prediction.value().sorted_predicted_urls()) {
    // Don't prefetch anything for Google, i.e.: same origin.
    if (google_util::IsGoogleAssociatedDomainUrl(url)) {
      continue;
    }

    if (url.HostIsIPAddress()) {
      continue;
    }

    if (!url.SchemeIs(url::kHttpsScheme)) {
      continue;
    }

    content::StoragePartition* storage_partition =
        content::BrowserContext::GetStoragePartitionForSite(
            profile_, url, /*can_create=*/false);
    net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
    storage_partition->GetCookieManagerForBrowserProcess()->GetCookieList(
        url, options,
        base::BindOnce(&IsolatedPrerenderTabHelper::OnGotCookieList,
                       weak_factory_.GetWeakPtr(), url));
  }
}

void IsolatedPrerenderTabHelper::OnGotCookieList(
    const GURL& url,
    const net::CookieStatusList& cookie_with_status_list,
    const net::CookieStatusList& excluded_cookies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!cookie_with_status_list.empty())
    return;

  urls_to_prefetch_.push_back(url);

  if (!PrefetchingActive()) {
    Prefetch();
    DCHECK(PrefetchingActive());
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(IsolatedPrerenderTabHelper)
