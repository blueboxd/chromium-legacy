// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_tab_helper.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_params.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_factory.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_workers_observer.h"
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
#include "net/base/network_isolation_key.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/origin.h"

IsolatedPrerenderTabHelper::CurrentPageLoad::CurrentPageLoad() = default;
IsolatedPrerenderTabHelper::CurrentPageLoad::~CurrentPageLoad() = default;

IsolatedPrerenderTabHelper::IsolatedPrerenderTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  page_ = std::make_unique<CurrentPageLoad>();
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
  page_->url_loader.reset();
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
  page_ = std::make_unique<CurrentPageLoad>();
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
IsolatedPrerenderTabHelper::TakePrefetchResponse(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = page_->prefetched_responses.find(url);
  if (it == page_->prefetched_responses.end())
    return nullptr;

  std::unique_ptr<PrefetchedMainframeResponseContainer> response =
      std::move(it->second);
  page_->prefetched_responses.erase(it);
  return response;
}

bool IsolatedPrerenderTabHelper::PrefetchingActive() const {
  return page_ && page_->url_loader;
}

void IsolatedPrerenderTabHelper::Prefetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly));

  page_->url_loader.reset();
  if (page_->urls_to_prefetch.empty()) {
    return;
  }

  if (IsolatedPrerenderMaximumNumberOfPrefetches().has_value() &&
      page_->num_prefetches_attempted >=
          IsolatedPrerenderMaximumNumberOfPrefetches().value()) {
    return;
  }
  page_->num_prefetches_attempted++;

  GURL url = page_->urls_to_prefetch[0];
  page_->urls_to_prefetch.erase(page_->urls_to_prefetch.begin());

  net::NetworkIsolationKey key =
      net::NetworkIsolationKey::CreateOpaqueAndNonTransient();
  network::ResourceRequest::TrustedParams trusted_params;
  trusted_params.network_isolation_key = key;

  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->method = "GET";
  request->load_flags = net::LOAD_DISABLE_CACHE | net::LOAD_PREFETCH;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->headers.SetHeader(content::kCorsExemptPurposeHeaderName, "prefetch");
  request->trusted_params = trusted_params;

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

  page_->url_loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);

  // base::Unretained is safe because |page_->url_loader| is owned by |this|.
  page_->url_loader->SetOnRedirectCallback(base::BindRepeating(
      &IsolatedPrerenderTabHelper::OnPrefetchRedirect, base::Unretained(this)));
  page_->url_loader->SetAllowHttpErrorResults(true);
  page_->url_loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&IsolatedPrerenderTabHelper::OnPrefetchComplete,
                     base::Unretained(this), url, key),
      1024 * 1024 * 5 /* 5MB */);
}

void IsolatedPrerenderTabHelper::OnPrefetchRedirect(
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  DCHECK(PrefetchingActive());
  // Run the new URL through all the eligibility checks. In the mean time,
  // continue on with other Prefetches.
  if (CheckAndMaybePrefetchURL(redirect_info.new_url)) {
    // The redirect shouldn't count against our prefetch limit if the redirect
    // was followed.
    page_->num_prefetches_attempted--;
  }
  // Cancels the current request.
  Prefetch();
}

void IsolatedPrerenderTabHelper::OnPrefetchComplete(
    const GURL& url,
    const net::NetworkIsolationKey& key,
    std::unique_ptr<std::string> body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(PrefetchingActive());

  if (page_->url_loader->NetError() == net::OK && body &&
      page_->url_loader->ResponseInfo()) {
    network::mojom::URLResponseHeadPtr head =
        page_->url_loader->ResponseInfo()->Clone();
    HandlePrefetchResponse(url, key, std::move(head), std::move(body));
  }
  Prefetch();
}

void IsolatedPrerenderTabHelper::HandlePrefetchResponse(
    const GURL& url,
    const net::NetworkIsolationKey& key,
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
  std::unique_ptr<PrefetchedMainframeResponseContainer> response =
      std::make_unique<PrefetchedMainframeResponseContainer>(
          key, std::move(head), std::move(body));
  page_->prefetched_responses.emplace(url, std::move(response));
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

  // This checks whether the user has enabled pre* actions in the settings UI.
  if (!chrome_browser_net::CanPreresolveAndPreconnectUI(profile_->GetPrefs())) {
    return;
  }

  // This is also checked before prefetching from the network, but checking
  // again here allows us to skip querying for cookies if we won't be
  // prefetching the url anyways.
  if (IsolatedPrerenderMaximumNumberOfPrefetches().has_value() &&
      page_->num_prefetches_attempted >=
          IsolatedPrerenderMaximumNumberOfPrefetches().value()) {
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
    CheckAndMaybePrefetchURL(url);
  }
}

bool IsolatedPrerenderTabHelper::CheckAndMaybePrefetchURL(const GURL& url) {
  DCHECK(data_reduction_proxy::DataReductionProxySettings::
             IsDataSaverEnabledByUser(profile_->IsOffTheRecord(),
                                      profile_->GetPrefs()));

  if (google_util::IsGoogleAssociatedDomainUrl(url)) {
    return false;
  }

  if (url.HostIsIPAddress()) {
    return false;
  }

  if (!url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  content::StoragePartition* default_storage_partition =
      content::BrowserContext::GetDefaultStoragePartition(profile_);

  // Only the default storage partition is supported since that is the only
  // place where service workers are observed by
  // |IsolatedPrerenderServiceWorkersObserver|.
  if (default_storage_partition !=
      content::BrowserContext::GetStoragePartitionForSite(
          profile_, url, /*can_create=*/false)) {
    return false;
  }

  IsolatedPrerenderService* isolated_prerender_service =
      IsolatedPrerenderServiceFactory::GetForProfile(profile_);
  if (!isolated_prerender_service) {
    return false;
  }

  base::Optional<bool> site_has_service_worker =
      isolated_prerender_service->service_workers_observer()
          ->IsServiceWorkerRegisteredForOrigin(url::Origin::Create(url));
  if (!site_has_service_worker.has_value() || site_has_service_worker.value()) {
    return false;
  }

  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  default_storage_partition->GetCookieManagerForBrowserProcess()->GetCookieList(
      url, options,
      base::BindOnce(&IsolatedPrerenderTabHelper::OnGotCookieList,
                     weak_factory_.GetWeakPtr(), url));
  return true;
}

void IsolatedPrerenderTabHelper::OnGotCookieList(
    const GURL& url,
    const net::CookieStatusList& cookie_with_status_list,
    const net::CookieStatusList& excluded_cookies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!cookie_with_status_list.empty())
    return;

  // TODO(robertogden): Consider adding redirect URLs to the front of the list.
  page_->urls_to_prefetch.push_back(url);

  if (!PrefetchingActive()) {
    Prefetch();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(IsolatedPrerenderTabHelper)
