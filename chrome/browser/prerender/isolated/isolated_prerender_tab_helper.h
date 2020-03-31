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
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/prerender/isolated/prefetched_mainframe_response_container.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

class Profile;

namespace net {
class NetworkIsolationKey;
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

  // Simple container for several metrics which pertain to prefetching actions
  // on a Google SRP.
  struct PrefetchMetrics {
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
    int64_t ordered_eligible_pages_bitmask = 0;

    // The number of SRP links that were eligible to be prefetched.
    size_t prefetch_eligible_count = 0;

    // The number of eligible prefetches that were attempted.
    size_t prefetch_attempted_count = 0;

    // The number of attempted prefetches that were successful (net error was OK
    // and HTTP response code was 2XX).
    size_t prefetch_successful_count = 0;
  };

  const PrefetchMetrics& metrics() const { return page_->metrics_; }

  void CallHandlePrefetchResponseForTesting(
      const GURL& url,
      const net::NetworkIsolationKey& key,
      network::mojom::URLResponseHeadPtr head,
      std::unique_ptr<std::string> body);

  // content::WebContentsObserver implementation.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Takes ownership of a prefetched response by URL, if one if available.
  std::unique_ptr<PrefetchedMainframeResponseContainer> TakePrefetchResponse(
      const GURL& url);

 protected:
  // Exposed for testing.
  explicit IsolatedPrerenderTabHelper(content::WebContents* web_contents);
  virtual network::mojom::URLLoaderFactory* GetURLLoaderFactory();

 private:
  friend class content::WebContentsUserData<IsolatedPrerenderTabHelper>;

  // Owns all per-pageload state in the tab helper so that new navigations only
  // need to reset an instance of this class to clean up previous state.
  class CurrentPageLoad {
   public:
    CurrentPageLoad();
    ~CurrentPageLoad();

    // The metrics pertaining to prefetching actions on a Google SRP page.
    PrefetchMetrics metrics_;

    // A map of all predicted URLs to their original placement in the ordered
    // prediction.
    std::map<GURL, size_t> original_prediction_ordering_;

    // The url loader that does all the prefetches. Set only when active.
    std::unique_ptr<network::SimpleURLLoader> url_loader_;

    // An ordered list of the URLs to prefetch.
    std::vector<GURL> urls_to_prefetch_;

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
  void OnPrefetchRedirect(const net::RedirectInfo& redirect_info,
                          const network::mojom::URLResponseHead& response_head,
                          std::vector<std::string>* removed_headers);

  // Called when |url_loader_| completes. |url| is the url that was requested
  // and |key| is the temporary NIK used during the request.
  void OnPrefetchComplete(const GURL& url,
                          const net::NetworkIsolationKey& key,
                          std::unique_ptr<std::string> response_body);

  // Checks the response from |OnPrefetchComplete| for success or failure. On
  // success the response is moved to a |PrefetchedMainframeResponseContainer|
  // and cached in |prefetched_responses_|.
  void HandlePrefetchResponse(const GURL& url,
                              const net::NetworkIsolationKey& key,
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

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IsolatedPrerenderTabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  IsolatedPrerenderTabHelper(const IsolatedPrerenderTabHelper&) = delete;
  IsolatedPrerenderTabHelper& operator=(const IsolatedPrerenderTabHelper&) =
      delete;
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_TAB_HELPER_H_
