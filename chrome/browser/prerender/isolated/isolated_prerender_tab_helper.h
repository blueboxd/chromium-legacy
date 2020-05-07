// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_TAB_HELPER_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_TAB_HELPER_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/prerender/isolated/prefetched_mainframe_response_container.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

class IsolatedPrerenderPageLoadMetricsObserver;
class Profile;

namespace net {
class IsolationInfo;
}  // namespace net

namespace network {
class SimpleURLLoader;
}  // namespace network

// This class listens to predictions of the next navigation and prefetches the
// mainpage content of Google Search Result Page links when they are available.
class IsolatedPrerenderTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<IsolatedPrerenderTabHelper>,
      public NavigationPredictorKeyedService::Observer {
 public:
  ~IsolatedPrerenderTabHelper() override;

  // A key to identify prefetching likely events to PLM.
  static const void* PrefetchingLikelyEventKey();

  class Observer {
   public:
    // Called when a prefetch for |url| is completed successfully.
    virtual void OnPrefetchCompletedSuccessfully(const GURL& url) {}

    // Called when a prefetch for |url| is completed with an HTTP error code
    // (non-2XX).
    virtual void OnPrefetchCompletedWithError(const GURL& url, int code) {}
  };

  // The various states that a prefetch can go through or terminate with. Used
  // in UKM logging so don't remove or reorder values.
  enum class PrefetchStatus {
    // The interceptor used a prefetch.
    kPrefetchUsedNoProbe = 0,

    // The interceptor used a prefetch after successfully probing the origin.
    kPrefetchUsedProbeSuccess = 1,

    // The interceptor was not able to use an available prefetch because the
    // origin probe failed.
    kPrefetchNotUsedProbeFailed = 2,

    // The url was eligible to be prefetched, but the network request was never
    // made.
    kPrefetchNotStarted = 3,

    // The url was not eligible to be prefetched because it is a Google-owned
    // domain.
    kPrefetchNotEligibleGoogleDomain = 4,

    // The url was not eligible to be prefetched because the user had cookies
    // for that origin.
    kPrefetchNotEligibleUserHasCookies = 5,

    // The url was not eligible to be prefetched because there was a registered
    // service worker for that origin.
    kPrefetchNotEligibleUserHasServiceWorker = 6,

    // The url was not eligible to be prefetched because its scheme was not
    // https://.
    kPrefetchNotEligibleSchemeIsNotHttps = 7,

    // The url was not eligible to be prefetched because its host was an IP
    // address.
    kPrefetchNotEligibleHostIsIPAddress = 8,

    // The url was not eligible to be prefetched because it uses a non-default
    // storage partition.
    kPrefetchNotEligibleNonDefaultStoragePartition = 9,

    // The network request was cancelled before it finished. This happens when
    // there is a new navigation.
    kPrefetchNotFinishedInTime = 10,

    // The prefetch failed because of a net error.
    kPrefetchFailedNetError = 11,

    // The prefetch failed with a non-2XX HTTP response code.
    kPrefetchFailedNon2XX = 12,

    // The prefetch's Content-Type header was not html.
    kPrefetchFailedNotHTML = 13,

    // The prefetch finished successfully but was never used.
    kPrefetchSuccessful = 14,

    // The navigation off of the Google SRP was to a url that was not on the
    // SRP.
    kNavigatedToLinkNotOnSRP = 15,
  };

  // Container for several metrics which pertain to prefetching actions
  // on a Google SRP. RefCounted to allow TabHelper's friend classes to monitor
  // metrics without needing a callback for every event.
  class PrefetchMetrics : public base::RefCounted<PrefetchMetrics> {
   public:
    PrefetchMetrics();

    // This bitmask keeps track each eligible page's placement in the original
    // navigation prediction. The Nth-LSB is set if the Nth predicted page is
    // eligible. Pages are in descending order of likelihood of user clicking.
    // For example, if the following prediction is made:
    //
    //   [eligible, not eligible, eligible, eligible]
    //
    // then the resulting bitmask will be
    //
    //   0b1101.
    int64_t ordered_eligible_pages_bitmask_ = 0;

    // The number of SRP links that were predicted. Only set on Google SRP pages
    // for eligible users. This should be used as the source of truth for
    // determining if the previous page was a Google SRP that could have had
    // prefetching actions.
    size_t predicted_urls_count_ = 0;

    // The number of SRP links that were eligible to be prefetched.
    size_t prefetch_eligible_count_ = 0;

    // The number of eligible prefetches that were attempted.
    size_t prefetch_attempted_count_ = 0;

    // The number of attempted prefetches that were successful (net error was OK
    // and HTTP response code was 2XX).
    size_t prefetch_successful_count_ = 0;

    // The total number of redirects encountered during all prefetches.
    size_t prefetch_total_redirect_count_ = 0;

    // The duration between navigation start and the start of prefetching.
    base::Optional<base::TimeDelta> navigation_to_prefetch_start_;

   private:
    friend class base::RefCounted<PrefetchMetrics>;
    ~PrefetchMetrics();
  };

  // Records metrics on the page load after a Google SRP for eligible users.
  class AfterSRPMetrics {
   public:
    AfterSRPMetrics();
    AfterSRPMetrics(const AfterSRPMetrics& other);
    ~AfterSRPMetrics();

    // The url of the page following the Google SRP.
    GURL url_;

    // The number of SRP links that were eligible to be prefetched on the SRP.
    // This is copied from the same named member of |srp_metrics_|.
    size_t prefetch_eligible_count_ = 0;

    // The position of the link on the SRP that was navigated to. Not set if the
    // navigated page wasn't in the SRP.
    base::Optional<size_t> clicked_link_srp_position_;

    // The status of a prefetch done on the SRP that may have been used here.
    base::Optional<PrefetchStatus> prefetch_status_;

    // The amount of time it took the probe to complete. Set only when a
    // prefetch is used and a probe was required.
    base::Optional<base::TimeDelta> probe_latency_;
  };

  const PrefetchMetrics& srp_metrics() const { return *(page_->srp_metrics_); }

  // Returns nullopt unless the previous page load was a Google SRP where |this|
  // got parsed SRP links from NavigationPredictor.
  base::Optional<IsolatedPrerenderTabHelper::AfterSRPMetrics>
  after_srp_metrics() const;

  // content::WebContentsObserver implementation.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Takes ownership of a prefetched response by URL, if one if available.
  std::unique_ptr<PrefetchedMainframeResponseContainer> TakePrefetchResponse(
      const GURL& url);

  // Updates |prefetch_status_by_url_|.
  void OnPrefetchStatusUpdate(const GURL& url, PrefetchStatus usage);

  // Called by the URLLoaderInterceptor to update |page_.probe_latency_|.
  void NotifyPrefetchProbeLatency(base::TimeDelta probe_latency);

  void AddObserverForTesting(Observer* observer);
  void RemoveObserverForTesting(Observer* observer);

 protected:
  // Exposed for testing.
  explicit IsolatedPrerenderTabHelper(content::WebContents* web_contents);
  virtual network::mojom::URLLoaderFactory* GetURLLoaderFactory();

 private:
  friend class IsolatedPrerenderPageLoadMetricsObserver;
  friend class content::WebContentsUserData<IsolatedPrerenderTabHelper>;

  // Owns all per-pageload state in the tab helper so that new navigations only
  // need to reset an instance of this class to clean up previous state.
  class CurrentPageLoad {
   public:
    explicit CurrentPageLoad(content::NavigationHandle* handle);
    ~CurrentPageLoad();

    // The start time of the current navigation.
    const base::TimeTicks navigation_start_;

    // The metrics pertaining to prefetching actions on a Google SRP page.
    scoped_refptr<PrefetchMetrics> srp_metrics_;

    // The metrics pertaining to how the prefetch is used after the Google SRP.
    // Only set for pages after a Google SRP.
    std::unique_ptr<AfterSRPMetrics> after_srp_metrics_;

    // The status of each prefetch.
    std::map<GURL, PrefetchStatus> prefetch_status_by_url_;

    // A map of all predicted URLs to their original placement in the ordered
    // prediction.
    std::map<GURL, size_t> original_prediction_ordering_;

    // The url loader that does all the prefetches. Set only when active.
    std::unique_ptr<network::SimpleURLLoader> url_loader_;

    // An ordered list of the URLs to prefetch.
    std::vector<GURL> urls_to_prefetch_;

    // The amount of time that the probe took to complete. Kept in this class
    // until commit in order to be plumbed into |AfterSRPMetrics|.
    base::Optional<base::TimeDelta> probe_latency_;

    // All prefetched responses by URL. This is cleared every time a mainframe
    // navigation commits.
    std::map<GURL, std::unique_ptr<PrefetchedMainframeResponseContainer>>
        prefetched_responses_;

    // The network context and url loader factory that will be used for
    // prefetches. A separate network context is used so that the prefetch proxy
    // can be used via a custom proxy configuration.
    mojo::Remote<network::mojom::URLLoaderFactory> isolated_url_loader_factory_;
    mojo::Remote<network::mojom::NetworkContext> isolated_network_context_;
  };

  // A helper method to make it easier to tell when prefetching is already
  // active.
  bool PrefetchingActive() const;

  // Prefetches the front of |urls_to_prefetch_|.
  void Prefetch();

  // Called when |url_loader_| encounters a redirect.
  void OnPrefetchRedirect(const GURL& original_url,
                          const net::RedirectInfo& redirect_info,
                          const network::mojom::URLResponseHead& response_head,
                          std::vector<std::string>* removed_headers);

  // Called when |url_loader_| completes. |url| is the url that was requested
  // and |key| is the temporary NIK used during the request.
  void OnPrefetchComplete(const GURL& url,
                          const net::IsolationInfo& isolation_info,
                          std::unique_ptr<std::string> response_body);

  // Checks the response from |OnPrefetchComplete| for success or failure. On
  // success the response is moved to a |PrefetchedMainframeResponseContainer|
  // and cached in |prefetched_responses_|.
  void HandlePrefetchResponse(const GURL& url,
                              const net::IsolationInfo& isolation_info,
                              network::mojom::URLResponseHeadPtr head,
                              std::unique_ptr<std::string> body);

  // NavigationPredictorKeyedService::Observer:
  void OnPredictionUpdated(
      const base::Optional<NavigationPredictorKeyedService::Prediction>
          prediction) override;

  // Runs |url| through all the eligibility checks and appends it to
  // |urls_to_prefetch_| if eligible and returns true. If not eligible, returns
  // false.
  bool CheckAndMaybePrefetchURL(const GURL& url);

  // Callback for each eligible prediction URL when their cookie list is known.
  // Only urls with no cookies will be prefetched.
  void OnGotCookieList(const GURL& url,
                       const net::CookieStatusList& cookie_with_status_list,
                       const net::CookieStatusList& excluded_cookies);

  // Creates the isolated network context and url loader factory for this page.
  void CreateIsolatedURLLoaderFactory();

  Profile* profile_;

  // Owns all members which need to be reset on a new page load.
  std::unique_ptr<CurrentPageLoad> page_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IsolatedPrerenderTabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  IsolatedPrerenderTabHelper(const IsolatedPrerenderTabHelper&) = delete;
  IsolatedPrerenderTabHelper& operator=(const IsolatedPrerenderTabHelper&) =
      delete;
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_TAB_HELPER_H_
