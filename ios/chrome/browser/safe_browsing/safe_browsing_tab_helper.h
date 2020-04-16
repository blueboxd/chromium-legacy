// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_H_

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"

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
  // TODO(crbug.com/1028755): Use the result of Safe Browsing queries to block
  // navigations that are identified as unsafe.
  class UrlCheckerClient : public base::SupportsWeakPtr<UrlCheckerClient> {
   public:
    UrlCheckerClient();
    ~UrlCheckerClient();

    UrlCheckerClient(const UrlCheckerClient&) = delete;
    UrlCheckerClient& operator=(const UrlCheckerClient&) = delete;

    // Queries the database using the given |url_checker|, for a request with
    // the given |url| and the given HTTP |method|.
    void CheckUrl(
        std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> url_checker,
        const GURL& url,
        const std::string& method);

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

    // Set of SafeBrowsingUrlCheckerImpls that have started but not completed
    // a url check.
    base::flat_set<std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl>,
                   base::UniquePtrComparator>
        active_url_checkers_;
  };

  // A WebStatePolicyDecider that queries the SafeBrowsing database on each
  // request, but always allows the request.
  // TODO(crbug.com/1028755): Use the result of Safe Browsing queries to block
  // navigations that are identified as unsafe.
  class PolicyDecider : public web::WebStatePolicyDecider {
   public:
    PolicyDecider(web::WebState* web_state,
                  UrlCheckerClient* url_checker_client);

    // web::WebStatePolicyDecider implementation
    web::WebStatePolicyDecider::PolicyDecision ShouldAllowRequest(
        NSURLRequest* request,
        const web::WebStatePolicyDecider::RequestInfo& request_info) override;

   private:
    UrlCheckerClient* url_checker_client_;
  };

  explicit SafeBrowsingTabHelper(web::WebState* web_state);

  std::unique_ptr<UrlCheckerClient> url_checker_client_;
  std::unique_ptr<PolicyDecider> policy_decider_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_H_
