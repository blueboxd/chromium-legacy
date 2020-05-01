// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/browser_url_loader_throttle.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/safe_browsing/core/realtime/policy_engine.h"
#include "components/safe_browsing/core/realtime/url_lookup_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/web_contents.h"
#include "net/log/net_log_event_type.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

// TODO(http://crbug.com/824843): Remove this if safe browsing is moved to the
// UI thread.
class BrowserURLLoaderThrottle::CheckerOnIO
    : public base::SupportsWeakPtr<BrowserURLLoaderThrottle::CheckerOnIO> {
 public:
  CheckerOnIO(
      GetDelegateCallback delegate_getter,
      int frame_tree_node_id,
      base::RepeatingCallback<content::WebContents*()> web_contents_getter,
      base::WeakPtr<BrowserURLLoaderThrottle> throttle,
      bool real_time_lookup_enabled,
      bool enhanced_protection_enabled,
      base::WeakPtr<RealTimeUrlLookupService> url_lookup_service)
      : delegate_getter_(std::move(delegate_getter)),
        frame_tree_node_id_(frame_tree_node_id),
        web_contents_getter_(web_contents_getter),
        throttle_(std::move(throttle)),
        real_time_lookup_enabled_(real_time_lookup_enabled),
        enhanced_protection_enabled_(enhanced_protection_enabled),
        url_lookup_service_(url_lookup_service) {}

  // Starts the initial safe browsing check. This check and future checks may be
  // skipped after checking with the UrlCheckerDelegate.
  void Start(const net::HttpRequestHeaders& headers,
             int load_flags,
             blink::mojom::ResourceType resource_type,
             bool has_user_gesture,
             bool originated_from_service_worker,
             const GURL& url,
             const std::string& method) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    scoped_refptr<UrlCheckerDelegate> url_checker_delegate =
        std::move(delegate_getter_).Run();
    skip_checks_ =
        !url_checker_delegate ||
        !url_checker_delegate->GetDatabaseManager()->IsSupported() ||
        url_checker_delegate->ShouldSkipRequestCheck(
            url, frame_tree_node_id_, -1 /* render_process_id */,
            -1 /* render_frame_id */, originated_from_service_worker);
    if (skip_checks_) {
      base::PostTask(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(&BrowserURLLoaderThrottle::SkipChecks, throttle_));
      return;
    }

    url_checker_ = std::make_unique<SafeBrowsingUrlCheckerImpl>(
        headers, load_flags, resource_type, has_user_gesture,
        url_checker_delegate, web_contents_getter_, real_time_lookup_enabled_,
        enhanced_protection_enabled_, url_lookup_service_);

    CheckUrl(url, method);
  }

  // Checks the specified |url| using |url_checker_|.
  void CheckUrl(const GURL& url, const std::string& method) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    if (skip_checks_) {
      base::PostTask(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(&BrowserURLLoaderThrottle::SkipChecks, throttle_));
      return;
    }

    DCHECK(url_checker_);
    url_checker_->CheckUrl(
        url, method,
        base::BindOnce(&BrowserURLLoaderThrottle::CheckerOnIO::OnCheckUrlResult,
                       base::Unretained(this)));
  }

 private:
  // If |slow_check_notifier| is non-null, it indicates that a "slow check" is
  // ongoing, i.e., the URL may be unsafe and a more time-consuming process is
  // required to get the final result. In that case, the rest of the callback
  // arguments should be ignored. This method sets the |slow_check_notifier|
  // output parameter to a callback to receive the final result.
  void OnCheckUrlResult(NativeUrlCheckNotifier* slow_check_notifier,
                        bool proceed,
                        bool showed_interstitial) {
    if (!slow_check_notifier) {
      OnCompleteCheck(false /* slow_check */, proceed, showed_interstitial);
      return;
    }

    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&BrowserURLLoaderThrottle::NotifySlowCheck, throttle_));

    // In this case |proceed| and |showed_interstitial| should be ignored. The
    // result will be returned by calling |*slow_check_notifier| callback.
    *slow_check_notifier =
        base::BindOnce(&BrowserURLLoaderThrottle::CheckerOnIO::OnCompleteCheck,
                       base::Unretained(this), true /* slow_check */);
  }

  // |slow_check| indicates whether it reports the result of a slow check.
  // (Please see comments of OnCheckUrlResult() for what slow check means).
  void OnCompleteCheck(bool slow_check,
                       bool proceed,
                       bool showed_interstitial) {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&BrowserURLLoaderThrottle::OnCompleteCheck, throttle_,
                       slow_check, proceed, showed_interstitial));
  }

  // The following member stays valid until |url_checker_| is created.
  GetDelegateCallback delegate_getter_;

  std::unique_ptr<SafeBrowsingUrlCheckerImpl> url_checker_;
  int frame_tree_node_id_;
  base::RepeatingCallback<content::WebContents*()> web_contents_getter_;
  bool skip_checks_ = false;
  base::WeakPtr<BrowserURLLoaderThrottle> throttle_;
  bool real_time_lookup_enabled_ = false;
  bool enhanced_protection_enabled_ = false;
  base::WeakPtr<RealTimeUrlLookupService> url_lookup_service_;
};

// static
std::unique_ptr<BrowserURLLoaderThrottle> BrowserURLLoaderThrottle::Create(
    GetDelegateCallback delegate_getter,
    const base::RepeatingCallback<content::WebContents*()>& web_contents_getter,
    int frame_tree_node_id,
    base::WeakPtr<RealTimeUrlLookupService> url_lookup_service) {
  return base::WrapUnique<BrowserURLLoaderThrottle>(
      new BrowserURLLoaderThrottle(std::move(delegate_getter),
                                   web_contents_getter, frame_tree_node_id,
                                   url_lookup_service));
}

BrowserURLLoaderThrottle::BrowserURLLoaderThrottle(
    GetDelegateCallback delegate_getter,
    const base::RepeatingCallback<content::WebContents*()>& web_contents_getter,
    int frame_tree_node_id,
    base::WeakPtr<RealTimeUrlLookupService> url_lookup_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Decide whether to do real time URL lookups or not.
  bool real_time_lookup_enabled =
      url_lookup_service ? url_lookup_service->CanPerformFullURLLookup()
                         : false;

  bool enhanced_protection_enabled =
      url_lookup_service && url_lookup_service->IsUserEpOptedIn();

  io_checker_ = std::make_unique<CheckerOnIO>(
      std::move(delegate_getter), frame_tree_node_id, web_contents_getter,
      weak_factory_.GetWeakPtr(), real_time_lookup_enabled,
      enhanced_protection_enabled, url_lookup_service);
}

BrowserURLLoaderThrottle::~BrowserURLLoaderThrottle() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (deferred_)
    TRACE_EVENT_ASYNC_END0("safe_browsing", "Deferred", this);

  DeleteCheckerOnIO();
}

void BrowserURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(0u, pending_checks_);
  DCHECK(!blocked_);

  original_url_ = request->url;
  pending_checks_++;
  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &BrowserURLLoaderThrottle::CheckerOnIO::Start,
          io_checker_->AsWeakPtr(), request->headers, request->load_flags,
          static_cast<blink::mojom::ResourceType>(request->resource_type),
          request->has_user_gesture, request->originated_from_service_worker,
          request->url, request->method));
}

void BrowserURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& /* response_head */,
    bool* defer,
    std::vector<std::string>* /* to_be_removed_headers */,
    net::HttpRequestHeaders* /* modified_headers */,
    net::HttpRequestHeaders* /* modified_cors_exempt_headers */) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (blocked_) {
    // OnCheckUrlResult() has set |blocked_| to true and called
    // |delegate_->CancelWithError|, but this method is called before the
    // request is actually cancelled. In that case, simply defer the request.
    *defer = true;
    return;
  }

  if (skip_checks_)
    return;

  pending_checks_++;
  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&BrowserURLLoaderThrottle::CheckerOnIO::CheckUrl,
                     io_checker_->AsWeakPtr(), redirect_info->new_url,
                     redirect_info->new_method));
}

void BrowserURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (blocked_) {
    // OnCheckUrlResult() has set |blocked_| to true and called
    // |delegate_->CancelWithError|, but this method is called before the
    // request is actually cancelled. In that case, simply defer the request.
    *defer = true;
    return;
  }

  if (pending_checks_ == 0)
    return;

  DCHECK(!deferred_);
  deferred_ = true;
  defer_start_time_ = base::TimeTicks::Now();
  *defer = true;
  TRACE_EVENT_ASYNC_BEGIN1("safe_browsing", "Deferred", this, "original_url",
                           original_url_.spec());
}

void BrowserURLLoaderThrottle::OnCompleteCheck(bool slow_check,
                                               bool proceed,
                                               bool showed_interstitial) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!blocked_);

  DCHECK_LT(0u, pending_checks_);
  pending_checks_--;

  if (slow_check) {
    DCHECK_LT(0u, pending_slow_checks_);
    pending_slow_checks_--;
  }

  user_action_involved_ = user_action_involved_ || showed_interstitial;
  // If the resource load is currently deferred and is going to exit that state
  // (either being cancelled or resumed), record the total delay.
  if (deferred_ && (!proceed || pending_checks_ == 0))
    total_delay_ = base::TimeTicks::Now() - defer_start_time_;

  if (proceed) {
    if (pending_slow_checks_ == 0 && slow_check)
      delegate_->ResumeReadingBodyFromNet();

    if (pending_checks_ == 0 && deferred_) {
      deferred_ = false;
      TRACE_EVENT_ASYNC_END0("safe_browsing", "Deferred", this);
      delegate_->Resume();
    }
  } else {
    blocked_ = true;

    DeleteCheckerOnIO();
    pending_checks_ = 0;
    pending_slow_checks_ = 0;
    // If we didn't show an interstitial, we cancel with ERR_ABORTED to not show
    // an error page either.
    delegate_->CancelWithError(
        showed_interstitial ? kNetErrorCodeForSafeBrowsing : net::ERR_ABORTED,
        kCustomCancelReasonForURLLoader);
  }
}

void BrowserURLLoaderThrottle::SkipChecks() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Future checks for redirects will be skipped.
  skip_checks_ = true;

  pending_checks_--;
  if (pending_checks_ == 0 && deferred_)
    delegate_->Resume();
}

void BrowserURLLoaderThrottle::NotifySlowCheck() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pending_slow_checks_++;

  // Pending slow checks indicate that the resource may be unsafe. In that case,
  // pause reading response body from network to minimize the chance of
  // processing unsafe contents (e.g., writing unsafe contents into cache),
  // until we get the results. According to the results, we may resume reading
  // or cancel the resource load.
  // For real time Safe Browsing checks, we continue reading the response body
  // but, similar to hash-based checks, do not process it until we know it is
  // SAFE.
  if (pending_slow_checks_ == 1)
    delegate_->PauseReadingBodyFromNet();
}

void BrowserURLLoaderThrottle::DeleteCheckerOnIO() {
  base::DeleteSoon(FROM_HERE, {content::BrowserThread::IO},
                   std::move(io_checker_));
}

}  // namespace safe_browsing
