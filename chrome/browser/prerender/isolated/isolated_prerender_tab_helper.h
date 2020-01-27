// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_TAB_HELPER_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_TAB_HELPER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/prerender/isolated/prefetched_response_container.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

class Profile;

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

  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  size_t prefetched_responses_size_for_testing() const {
    return prefetched_responses_.size();
  }

  // content::WebContentsObserver implementation.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Takes ownership of a prefetched response by URL, if one if available.
  std::unique_ptr<PrefetchedResponseContainer> TakePrefetchResponse(
      const GURL& url);

 private:
  explicit IsolatedPrerenderTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<IsolatedPrerenderTabHelper>;

  // Prefetches the front of |urls_to_prefetch_|.
  void Prefetch();

  void OnPrefetchRedirect(const net::RedirectInfo& redirect_info,
                          const network::mojom::URLResponseHead& response_head,
                          std::vector<std::string>* removed_headers);

  void OnPrefetchComplete(const GURL& url,
                          std::unique_ptr<std::string> response_body);

  void HandlePrefetchResponse(const GURL& url,
                              network::mojom::URLResponseHeadPtr head,
                              std::unique_ptr<std::string> body);

  // NavigationPredictorKeyedService::Observer:
  void OnPredictionUpdated(
      const base::Optional<NavigationPredictorKeyedService::Prediction>&
          prediction) override;

  Profile* profile_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // TODO(robertogden): Consider encapsulating the per-page-load members below
  // into a separate object.

  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // An ordered list of the URLs to prefetch.
  std::vector<GURL> urls_to_prefetch_;

  // All prefetched responses by URL. This is cleared every time a mainframe
  // navigation commits.
  std::map<GURL, std::unique_ptr<PrefetchedResponseContainer>>
      prefetched_responses_;

  SEQUENCE_CHECKER(sequence_checker_);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  IsolatedPrerenderTabHelper(const IsolatedPrerenderTabHelper&) = delete;
  IsolatedPrerenderTabHelper& operator=(const IsolatedPrerenderTabHelper&) =
      delete;
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_TAB_HELPER_H_
