// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_H_

#include <list>

#include "base/containers/flat_map.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"
#include "url/gurl.h"

namespace safe_browsing {
class SafeBrowsingUrlCheckerImpl;
}

// A tab helper that uses Safe Browsing to check whether URLs that are being
// navigated to are unsafe.
class SafeBrowsingTabHelper
    : public web::WebStateUserData<SafeBrowsingTabHelper> {
 public:
  ~SafeBrowsingTabHelper() override;

  SafeBrowsingTabHelper(const SafeBrowsingTabHelper&) = delete;
  SafeBrowsingTabHelper& operator=(const SafeBrowsingTabHelper&) = delete;

 private:
  friend class web::WebStateUserData<SafeBrowsingTabHelper>;

  // Queries the Safe Browsing database using SafeBrowsingUrlCheckerImpls but
  // doesn't do anything with the result yet. This class may be constructed on
  // the UI thread but otherwise must only be used and destroyed on the IO
  // thread.
  class UrlCheckerClient : public base::SupportsWeakPtr<UrlCheckerClient> {
   public:
    UrlCheckerClient();
    ~UrlCheckerClient();

    UrlCheckerClient(const UrlCheckerClient&) = delete;
    UrlCheckerClient& operator=(const UrlCheckerClient&) = delete;

    // Queries the database using the given |url_checker|, for a request with
    // the given |url| and the given HTTP |method|. After receiving a result
    // from the database, runs the given |callback| on the UI thread with the
    // result.
    void CheckUrl(
        std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> url_checker,
        const GURL& url,
        const std::string& method,
        base::OnceCallback<void(web::WebStatePolicyDecider::PolicyDecision)>
            callback);

   private:
    // Called by |url_checker| with the initial result of performing a url
    // check. |url_checker| must be non-null. This is an implementation of
    // SafeBrowsingUrlCheckerImpl::NativeUrlCheckCallBack. |slow_check_notifier|
    // is an out-parameter; when a non-null value is passed in, it is set to a
    // callback that receives the final result of the url check.
    void OnCheckUrlResult(
        safe_browsing::SafeBrowsingUrlCheckerImpl* url_checker,
        safe_browsing::SafeBrowsingUrlCheckerImpl::NativeUrlCheckNotifier*
            slow_check_notifier,
        bool proceed,
        bool showed_interstitial);

    // Called by |url_checker| with the final result of performing a url check.
    // |url_checker| must be non-null. This is an implementation of
    // SafeBrowsingUrlCheckerImpl::NativeUrlCheckNotifier.
    void OnCheckComplete(safe_browsing::SafeBrowsingUrlCheckerImpl* url_checker,
                         bool proceed,
                         bool showed_interstitial);

    // This maps SafeBrowsingUrlCheckerImpls that have started but not completed
    // a url check to the callback that should be invoked once the url check is
    // complete.
    base::flat_map<
        std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl>,
        base::OnceCallback<void(web::WebStatePolicyDecider::PolicyDecision)>,
        base::UniquePtrComparator>
        active_url_checkers_;
  };

  // A WebStatePolicyDecider that queries the SafeBrowsing database on each
  // request, always allows the request, but uses the result of the
  // SafeBrowsing check to determine whether to allow the corresponding
  // response.
  class PolicyDecider : public web::WebStatePolicyDecider,
                        public base::SupportsWeakPtr<PolicyDecider> {
   public:
    PolicyDecider(web::WebState* web_state,
                  UrlCheckerClient* url_checker_client);

    ~PolicyDecider() override;

    // web::WebStatePolicyDecider implementation
    web::WebStatePolicyDecider::PolicyDecision ShouldAllowRequest(
        NSURLRequest* request,
        const web::WebStatePolicyDecider::RequestInfo& request_info) override;
    void ShouldAllowResponse(
        NSURLResponse* response,
        bool for_main_frame,
        base::OnceCallback<void(web::WebStatePolicyDecider::PolicyDecision)>
            callback) override;

   private:
    // Represents a single Safe Browsing query URL, along with the corresponding
    // decision once it's received, and the callback to invoke once the decision
    // is known.
    struct PendingUrlQuery {
      explicit PendingUrlQuery(const GURL& url);
      PendingUrlQuery(PendingUrlQuery&& query);
      ~PendingUrlQuery();

      GURL url;
      base::Optional<web::WebStatePolicyDecider::PolicyDecision> decision;
      base::OnceCallback<void(web::WebStatePolicyDecider::PolicyDecision)>
          response_callback;
    };

    // This is invoked with the Safe Browsing |decision| for |url|.
    void OnUrlQueryDecided(const GURL& url,
                           bool for_main_frame,
                           web::WebStatePolicyDecider::PolicyDecision decision);

    UrlCheckerClient* url_checker_client_;

    // A list of Safe Browsing queries for main frame URLs, where either the
    // decision is not yet known or ShouldAllowResponse() has not yet been
    // called for the URL. This list is maintained in the same order as calls
    // to ShouldAllowRequest().
    std::list<PendingUrlQuery> pending_main_frame_queries_;
  };

  explicit SafeBrowsingTabHelper(web::WebState* web_state);

  std::unique_ptr<UrlCheckerClient> url_checker_client_;
  PolicyDecider policy_decider_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_H_
