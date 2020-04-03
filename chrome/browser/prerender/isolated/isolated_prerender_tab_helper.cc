// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_tab_helper.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_params.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_proxy_configurator.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_factory.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_workers_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/google/core/common/google_util.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
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
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "url/origin.h"

namespace {

const void* const kPrefetchingLikelyEventKey = 0;

base::Optional<base::TimeDelta> GetTotalPrefetchTime(
    network::mojom::URLResponseHead* head) {
  DCHECK(head);

  base::Time start = head->request_time;
  base::Time end = head->response_time;

  if (start.is_null() || end.is_null())
    return base::nullopt;

  return end - start;
}

base::Optional<base::TimeDelta> GetPrefetchConnectTime(
    network::mojom::URLResponseHead* head) {
  DCHECK(head);

  base::TimeTicks start = head->load_timing.connect_timing.connect_start;
  base::TimeTicks end = head->load_timing.connect_timing.connect_end;

  if (start.is_null() || end.is_null())
    return base::nullopt;

  return end - start;
}

void InformPLMOfLikelyPrefetching(content::WebContents* web_contents) {
  page_load_metrics::MetricsWebContentsObserver* metrics_web_contents_observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents);
  if (!metrics_web_contents_observer)
    return;

  metrics_web_contents_observer->BroadcastEventToObservers(
      IsolatedPrerenderTabHelper::PrefetchingLikelyEventKey());
}

}  // namespace

IsolatedPrerenderTabHelper::PrefetchMetrics::PrefetchMetrics() = default;
IsolatedPrerenderTabHelper::PrefetchMetrics::~PrefetchMetrics() = default;

IsolatedPrerenderTabHelper::CurrentPageLoad::CurrentPageLoad()
    : metrics_(
          base::MakeRefCounted<IsolatedPrerenderTabHelper::PrefetchMetrics>()) {
}
IsolatedPrerenderTabHelper::CurrentPageLoad::~CurrentPageLoad() = default;

// static
const void* IsolatedPrerenderTabHelper::PrefetchingLikelyEventKey() {
  return &kPrefetchingLikelyEventKey;
}

IsolatedPrerenderTabHelper::IsolatedPrerenderTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  page_ = std::make_unique<CurrentPageLoad>();
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());

  NavigationPredictorKeyedService* navigation_predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile_);
  if (navigation_predictor_service) {
    navigation_predictor_service->AddObserver(this);
  }

  // Make sure the global service is up and running so that the service worker
  // registrations can be queried before the first navigation prediction.
  IsolatedPrerenderServiceFactory::GetForProfile(profile_);
}

IsolatedPrerenderTabHelper::~IsolatedPrerenderTabHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NavigationPredictorKeyedService* navigation_predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile_);
  if (navigation_predictor_service) {
    navigation_predictor_service->RemoveObserver(this);
  }
}

void IsolatedPrerenderTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }

  // Reset the prefetch usage here instead of with |page_| since this will be
  // set before commit.
  prefetch_usage_ = base::nullopt;

  // User is navigating, don't bother prefetching further.
  page_->url_loader_.reset();

  if (page_->metrics_->prefetch_attempted_count_ > 0) {
    UMA_HISTOGRAM_COUNTS_100(
        "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects",
        page_->metrics_->prefetch_total_redirect_count_);
  }
}

void IsolatedPrerenderTabHelper::OnPrefetchUsage(PrefetchUsage usage) {
  prefetch_usage_ = usage;
}

void IsolatedPrerenderTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }
  if (!navigation_handle->HasCommitted()) {
    return;
  }

  DCHECK(!PrefetchingActive());

  // |page_| is reset on commit so that any available cached prefetches that
  // result from a redirect get used.
  page_ = std::make_unique<CurrentPageLoad>();
}

void IsolatedPrerenderTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsolatedPrerenderIsEnabled()) {
    return;
  }

  // Start prefetching if the tab has become visible and prefetching is
  // inactive. Hidden and occluded visibility is ignored here so that pending
  // prefetches can finish.
  if (visibility == content::Visibility::VISIBLE && !PrefetchingActive())
    Prefetch();
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
IsolatedPrerenderTabHelper::TakePrefetchResponse(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = page_->prefetched_responses_.find(url);
  if (it == page_->prefetched_responses_.end())
    return nullptr;

  std::unique_ptr<PrefetchedMainframeResponseContainer> response =
      std::move(it->second);
  page_->prefetched_responses_.erase(it);
  return response;
}

bool IsolatedPrerenderTabHelper::PrefetchingActive() const {
  return page_ && page_->url_loader_;
}

void IsolatedPrerenderTabHelper::Prefetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsolatedPrerenderIsEnabled());

  page_->url_loader_.reset();
  if (page_->urls_to_prefetch_.empty()) {
    return;
  }

  if (IsolatedPrerenderMaximumNumberOfPrefetches().has_value() &&
      page_->metrics_->prefetch_attempted_count_ >=
          IsolatedPrerenderMaximumNumberOfPrefetches().value()) {
    return;
  }

  if (web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
    // |OnVisibilityChanged| will restart prefetching when the tab becomes
    // visible again.
    return;
  }

  page_->metrics_->prefetch_attempted_count_++;

  GURL url = page_->urls_to_prefetch_[0];
  page_->urls_to_prefetch_.erase(page_->urls_to_prefetch_.begin());

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

  page_->url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);

  // base::Unretained is safe because |page_->url_loader_| is owned by |this|.
  page_->url_loader_->SetOnRedirectCallback(base::BindRepeating(
      &IsolatedPrerenderTabHelper::OnPrefetchRedirect, base::Unretained(this)));
  page_->url_loader_->SetAllowHttpErrorResults(true);
  page_->url_loader_->SetTimeoutDuration(IsolatedPrefetchTimeoutDuration());
  page_->url_loader_->DownloadToString(
      GetURLLoaderFactory(),
      base::BindOnce(&IsolatedPrerenderTabHelper::OnPrefetchComplete,
                     base::Unretained(this), url, key),
      1024 * 1024 * 5 /* 5MB */);
}

void IsolatedPrerenderTabHelper::OnPrefetchRedirect(
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  DCHECK(PrefetchingActive());

  page_->metrics_->prefetch_total_redirect_count_++;

  // Run the new URL through all the eligibility checks. In the mean time,
  // continue on with other Prefetches.
  CheckAndMaybePrefetchURL(redirect_info.new_url);

  // Cancels the current request.
  Prefetch();
}

void IsolatedPrerenderTabHelper::OnPrefetchComplete(
    const GURL& url,
    const net::NetworkIsolationKey& key,
    std::unique_ptr<std::string> body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(PrefetchingActive());

  base::UmaHistogramSparse("IsolatedPrerender.Prefetch.Mainframe.NetError",
                           std::abs(page_->url_loader_->NetError()));

  if (page_->url_loader_->NetError() == net::OK && body &&
      page_->url_loader_->ResponseInfo()) {
    network::mojom::URLResponseHeadPtr head =
        page_->url_loader_->ResponseInfo()->Clone();

    DCHECK(!head->proxy_server.is_direct());

    HandlePrefetchResponse(url, key, std::move(head), std::move(body));
  }
  Prefetch();
}

void IsolatedPrerenderTabHelper::CallHandlePrefetchResponseForTesting(
    const GURL& url,
    const net::NetworkIsolationKey& key,
    network::mojom::URLResponseHeadPtr head,
    std::unique_ptr<std::string> body) {
  HandlePrefetchResponse(url, key, std::move(head), std::move(body));
}

void IsolatedPrerenderTabHelper::HandlePrefetchResponse(
    const GURL& url,
    const net::NetworkIsolationKey& key,
    network::mojom::URLResponseHeadPtr head,
    std::unique_ptr<std::string> body) {
  DCHECK(!head->was_fetched_via_cache);

  if (!head->headers)
    return;

  UMA_HISTOGRAM_COUNTS_10M("IsolatedPrerender.Prefetch.Mainframe.BodyLength",
                           body->size());

  base::Optional<base::TimeDelta> total_time = GetTotalPrefetchTime(head.get());
  if (total_time) {
    UMA_HISTOGRAM_CUSTOM_TIMES("IsolatedPrerender.Prefetch.Mainframe.TotalTime",
                               *total_time,
                               base::TimeDelta::FromMilliseconds(10),
                               base::TimeDelta::FromSeconds(30), 100);
  }

  base::Optional<base::TimeDelta> connect_time =
      GetPrefetchConnectTime(head.get());
  if (connect_time) {
    UMA_HISTOGRAM_TIMES("IsolatedPrerender.Prefetch.Mainframe.ConnectTime",
                        *connect_time);
  }

  int response_code = head->headers->response_code();

  base::UmaHistogramSparse("IsolatedPrerender.Prefetch.Mainframe.RespCode",
                           response_code);

  if (response_code < 200 || response_code >= 300) {
    return;
  }

  if (head->mime_type != "text/html") {
    return;
  }
  std::unique_ptr<PrefetchedMainframeResponseContainer> response =
      std::make_unique<PrefetchedMainframeResponseContainer>(
          key, std::move(head), std::move(body));
  page_->prefetched_responses_.emplace(url, std::move(response));
  page_->metrics_->prefetch_successful_count_++;
}

void IsolatedPrerenderTabHelper::OnPredictionUpdated(
    const base::Optional<NavigationPredictorKeyedService::Prediction>
        prediction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsolatedPrerenderIsEnabled()) {
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

  if (!prediction.has_value()) {
    return;
  }

  if (prediction->prediction_source() !=
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage) {
    return;
  }

  if (prediction.value().web_contents() != web_contents()) {
    // We only care about predictions in this tab.
    return;
  }

  const base::Optional<GURL>& source_document_url =
      prediction->source_document_url();

  if (!source_document_url || source_document_url->is_empty())
    return;

  if (!google_util::IsGoogleSearchUrl(source_document_url.value())) {
    return;
  }

  // It's very likely we'll prefetch something at this point, so inform PLM to
  // start tracking metrics.
  InformPLMOfLikelyPrefetching(web_contents());

  // It is possible, since it is not stipulated by the API contract, that the
  // navigation predictor will issue multiple predictions during a single page
  // load. Additional predictions should be treated as appending to the ordering
  // of previous predictions.
  size_t original_prediction_ordering_starting_size =
      page_->original_prediction_ordering_.size();

  for (size_t i = 0; i < prediction.value().sorted_predicted_urls().size();
       ++i) {
    GURL url = prediction.value().sorted_predicted_urls()[i];

    size_t url_index = original_prediction_ordering_starting_size + i;
    page_->original_prediction_ordering_.emplace(url, url_index);

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
  page_->urls_to_prefetch_.push_back(url);
  page_->metrics_->prefetch_eligible_count_++;

  // The queried url may not have been part of this page's prediction if it was
  // a redirect (common) or if the cookie query finished after
  // |OnFinishNavigation| (less common). Either way, don't record anything in
  // the bitmask.
  if (page_->original_prediction_ordering_.find(url) !=
      page_->original_prediction_ordering_.end()) {
    size_t original_prediction_index =
        page_->original_prediction_ordering_.find(url)->second;
    // Check that we won't go above the allowable size.
    if (original_prediction_index <
        sizeof(page_->metrics_->ordered_eligible_pages_bitmask_) * 8) {
      page_->metrics_->ordered_eligible_pages_bitmask_ |=
          1 << original_prediction_index;
    }
  }

  if (!PrefetchingActive()) {
    Prefetch();
  }
}

network::mojom::URLLoaderFactory*
IsolatedPrerenderTabHelper::GetURLLoaderFactory() {
  if (!page_->isolated_url_loader_factory_) {
    CreateIsolatedURLLoaderFactory();
  }
  DCHECK(page_->isolated_url_loader_factory_);
  return page_->isolated_url_loader_factory_.get();
}

void IsolatedPrerenderTabHelper::CreateIsolatedURLLoaderFactory() {
  page_->isolated_network_context_.reset();
  page_->isolated_url_loader_factory_.reset();

  IsolatedPrerenderService* isolated_prerender_service =
      IsolatedPrerenderServiceFactory::GetForProfile(profile_);

  auto context_params = network::mojom::NetworkContextParams::New();
  context_params->user_agent = ::GetUserAgent();
  context_params->initial_custom_proxy_config =
      isolated_prerender_service->proxy_configurator()
          ->CreateCustomProxyConfig();

  // Also register a client config receiver so that updates to the set of proxy
  // hosts or proxy headers will be updated.
  mojo::Remote<network::mojom::CustomProxyConfigClient> config_client;
  context_params->custom_proxy_config_client_receiver =
      config_client.BindNewPipeAndPassReceiver();
  isolated_prerender_service->proxy_configurator()->AddCustomProxyConfigClient(
      std::move(config_client));

  content::GetNetworkService()->CreateNetworkContext(
      page_->isolated_network_context_.BindNewPipeAndPassReceiver(),
      std::move(context_params));

  auto factory_params = network::mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = network::mojom::kBrowserProcessId;
  factory_params->is_trusted = true;
  factory_params->is_corb_enabled = false;

  page_->isolated_network_context_->CreateURLLoaderFactory(
      page_->isolated_url_loader_factory_.BindNewPipeAndPassReceiver(),
      std::move(factory_params));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(IsolatedPrerenderTabHelper)
