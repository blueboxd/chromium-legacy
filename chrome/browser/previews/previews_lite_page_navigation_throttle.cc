// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"

#include <stdint.h>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_lite_page_decider.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "components/base32/base32.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/referrer.h"
#include "crypto/sha2.h"
#include "net/base/escape.h"
#include "net/base/ip_address.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

constexpr char kChromeProxyHeader[] = "chrome-proxy";

const base::TimeDelta kBlacklistDuration = base::TimeDelta::FromDays(30);

content::OpenURLParams MakeOpenURLParams(content::NavigationHandle* handle,
                                         GURL url,
                                         const std::string& headers) {
  content::OpenURLParams url_params(
      url, handle->GetReferrer(), WindowOpenDisposition::CURRENT_TAB,
      handle->GetPageTransition(), handle->IsRendererInitiated());
  url_params.initiator_origin = handle->GetInitiatorOrigin();
  // crbug.com/916892: When a client redirect occurs on a site before the page
  // has finished loading, it is not considered a new NavigationEntry and so
  // clicking "Back" on the redirected page returns to the previous loaded page.
  // However, all browser-initiated navigations are assumed to be new
  // NavigationEntries (see NavigationControllerImpl::NavigateWithoutEntry). So
  // if the canceled navigation was a client redirect, and there is a previous
  // entry to go to, set |should_replace_current_entry|.
  url_params.should_replace_current_entry =
      (handle->GetPageTransition() & ui::PAGE_TRANSITION_CLIENT_REDIRECT) &&
      handle->IsRendererInitiated() &&
      handle->GetWebContents()->GetController().GetLastCommittedEntry();
  url_params.extra_headers = headers;
  url_params.redirect_chain = handle->GetRedirectChain();
  url_params.frame_tree_node_id = handle->GetFrameTreeNodeId();
  url_params.user_gesture = handle->HasUserGesture();
  url_params.started_from_context_menu = handle->WasStartedFromContextMenu();
  url_params.reload_type = handle->GetReloadType();
  return url_params;
}

}  // namespace

class PreviewsWebContentsLifetimeHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PreviewsWebContentsLifetimeHelper> {
 public:
  explicit PreviewsWebContentsLifetimeHelper(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        web_contents_(web_contents) {}

  // Keep track of all ongoing navigations in this WebContents.
  void DidStartNavigation(content::NavigationHandle* handle) override {
    DCHECK(handle);
    if (!handle->IsInMainFrame())
      return;

    navigations_.insert(handle);

    // Check if the starting navigation was reported as being caused by a
    // restart. Note: This could be a navigation to the litepages server, or to
    // the original URL.
    if (restarted_navigation_url_ == handle->GetURL()) {
      if (!info_) {
        // Create a new info_ if needed. This will use the previous page_id,
        // which is desired.
        PreviewsService* previews_service =
            PreviewsServiceFactory::GetForProfile(Profile::FromBrowserContext(
                web_contents()->GetBrowserContext()));
        PreviewsLitePageNavigationThrottleManager* manager =
            previews_service->previews_lite_page_decider();

        info_ =
            PreviewsLitePageNavigationThrottle::GetOrCreateServerLitePageInfo(
                handle, manager)
                ->Clone();
      }

      // Create a new PreviewsUserData if needed.
      PreviewsUITabHelper* ui_tab_helper =
          PreviewsUITabHelper::FromWebContents(web_contents());
      previews::PreviewsUserData* previews_data =
          ui_tab_helper->CreatePreviewsUserDataForNavigationHandle(
              handle, info_->page_id);
      previews_data->set_server_lite_page_info(std::move(info_));

      // Reset member state.
      restarted_navigation_url_ = GURL();
    }
  }

  // Keep track of all ongoing navigations in this WebContents.
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    DCHECK(handle);
    if (!handle->IsInMainFrame())
      return;

    if (navigations_.find(handle) != navigations_.end()) {
      navigations_.erase(handle);
    }

    // If this navigation is committing here, it has passed the Navigation
    // Throttle checks. Record time penalty UMA about the final state of the
    // navigation.
    PreviewsUITabHelper* ui_tab_helper =
        PreviewsUITabHelper::FromWebContents(web_contents());
    previews::PreviewsUserData* previews_data =
        ui_tab_helper->GetPreviewsUserData(handle);
    if (!handle->HasCommitted() || !previews_data ||
        !previews_data->server_lite_page_info()) {
      return;
    }

    previews::PreviewsUserData::ServerLitePageInfo* info =
        previews_data->server_lite_page_info();

    // Don't record this UMA for an unknown or control group status.
    if (info->status == previews::ServerLitePageStatus::kUnknown ||
        info->status == previews::ServerLitePageStatus::kControl) {
      return;
    }

    base::TimeDelta penalty =
        handle->NavigationStart() - info->original_navigation_start;

    base::LinearHistogram::FactoryTimeGet(
        base::StringPrintf(
            "Previews.ServerLitePage.Penalty.%s",
            previews::ServerLitePageStatusToString(info->status).c_str()),
        base::TimeDelta(), base::TimeDelta::FromMinutes(3), 50,
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(penalty.InMilliseconds());
  }

  // This method should be called after some delay to cancel an ongoing previews
  // navigation. This method checks if the ongoing navigation is for the given
  // |url|, if so the |fallback_callback| is run.
  void CheckForHungNavigation(const GURL& url,
                              base::OnceClosure fallback_callback) {
    DCHECK_GE(2u, navigations_.size());
    if (navigations_.empty())
      return;

    content::NavigationHandle* handle = *navigations_.begin();
    if (handle->GetURL() != url)
      return;

    UMA_HISTOGRAM_ENUMERATION(
        "Previews.ServerLitePage.ServerResponse",
        PreviewsLitePageNavigationThrottle::ServerResponse::kTimeout);

    std::move(fallback_callback).Run();
  }

  base::WeakPtr<PreviewsWebContentsLifetimeHelper> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void PostNewNavigation(
      const content::OpenURLParams& url_params,
      std::unique_ptr<previews::PreviewsUserData::ServerLitePageInfo> info) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(url_params.url.is_valid());
    DCHECK(url_params.url.SchemeIs(url::kHttpsScheme));
    // Setting these members should always happen before |OpenURL| which can be
    // synchronous.
    restarted_navigation_url_ = url_params.url;
    info_ = std::move(info);
    if (info_)
      info_->restart_count++;

    web_contents_->OpenURL(url_params);
  }

 private:
  friend class content::WebContentsUserData<PreviewsWebContentsLifetimeHelper>;
  // The url to monitor for. When it is seen, |info_| will be attached to that
  // navigation.
  GURL restarted_navigation_url_;

  // The ServerLitePageInfo to attach to the next navigation that matches
  // |restarted_navigation_url_|.
  std::unique_ptr<previews::PreviewsUserData::ServerLitePageInfo> info_;

  content::WebContents* web_contents_;
  std::unordered_set<content::NavigationHandle*> navigations_;
  base::WeakPtrFactory<PreviewsWebContentsLifetimeHelper> weak_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(PreviewsWebContentsLifetimeHelper)

bool HandlePreviewsLitePageURLRewrite(
    GURL* url,
    content::BrowserContext* browser_context) {
  // Don't change the |url|, just register our interest in reversing it before
  // it is displayed to the user in |HandlePreviewsLitePageURLRewriteReverse|.
  // Without returning true here, |HandlePreviewsLitePageURLRewriteReverse|
  // would not be called.

  auto* data_reduction_proxy_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context);

  if (!data_reduction_proxy_settings)
    return false;

  return data_reduction_proxy_settings->IsDataReductionProxyEnabled() &&
         previews::params::IsLitePageServerPreviewsEnabled();
}

bool HandlePreviewsLitePageURLRewriteReverse(
    GURL* url,
    content::BrowserContext* browser_context) {
  std::string original_url;
  if (previews::ExtractOriginalURLFromLitePageRedirectURL(*url,
                                                          &original_url)) {
    *url = GURL(original_url);
    return true;
  }
  return false;
}

PreviewsLitePageNavigationThrottle::PreviewsLitePageNavigationThrottle(
    content::NavigationHandle* handle,
    PreviewsLitePageNavigationThrottleManager* manager)
    : content::NavigationThrottle(handle), manager_(manager) {
  DCHECK(manager_);
  DCHECK(handle);
  DCHECK(handle->GetWebContents());
  DCHECK(handle->GetWebContents()->GetBrowserContext());
}

PreviewsLitePageNavigationThrottle::~PreviewsLitePageNavigationThrottle() =
    default;

// static
void PreviewsLitePageNavigationThrottle::LogIneligibleReason(
    IneligibleReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.IneligibleReasons",

                            reason);
}

bool PreviewsLitePageNavigationThrottle::IsEligibleForPreview() const {
  DCHECK(navigation_handle()->IsInMainFrame());

  if (data_reduction_proxy::HasURLRedirectCycle(
          navigation_handle()->GetRedirectChain()) ||
      (GetServerLitePageInfo() &&
       GetServerLitePageInfo()->restart_count >=
           previews::params::LitePageRedirectPreviewMaxNavigationRestarts())) {
    LogIneligibleReason(IneligibleReason::kExceededMaxNavigationRestarts);
    return false;
  }

  PreviewsUITabHelper* tab_helper = PreviewsUITabHelper::FromWebContents(
      navigation_handle()->GetWebContents());
  previews::PreviewsUserData* previews_data =
      tab_helper ? (tab_helper->GetPreviewsUserData(navigation_handle()))
                 : nullptr;

  if (!previews_data || !(previews_data->AllowedPreviewsState() &
                          content::LITE_PAGE_REDIRECT_ON)) {
    return false;
  }

  DCHECK_EQ(navigation_handle()->GetReloadType(), content::ReloadType::NONE);

  if (previews::IsLitePageRedirectPreviewDomain(navigation_handle()->GetURL()))
    return false;

  return true;
}

// static
GURL PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(
    const GURL& original_url) {
  DCHECK(original_url.is_valid());
  const std::string experiment_id =
      data_reduction_proxy::params::GetDataSaverServerExperiments();

  std::string experiment_query;
  if (!experiment_id.empty()) {
    experiment_query =
        "&x=" + net::EscapeQueryParamValue(experiment_id, true /* use_plus */);
  }
  std::string fragment;
  if (original_url.has_ref()) {
    fragment = "#" + original_url.ref();
  }

  // Strip out the fragment so that it is not sent to the server.
  std::string origin_hash = base::ToLowerASCII(base32::Base32Encode(
      crypto::SHA256HashString(
          original_url.scheme() + "://" + original_url.host() + ":" +
          base::NumberToString(original_url.EffectiveIntPort())),
      base32::Base32EncodePolicy::OMIT_PADDING));
  GURL previews_host = previews::params::GetLitePagePreviewsDomainURL();
  GURL previews_url = GURL(
      previews_host.scheme() + "://" + origin_hash + "." +
      previews_host.host() +
      (previews_host.has_port() ? (":" + previews_host.port()) : "") + "/p?u=" +
      net::EscapeQueryParamValue(original_url.GetAsReferrer().spec(),
                                 true /* use_plus */) +
      experiment_query + fragment);
  DCHECK(previews_url.is_valid());
  DCHECK_EQ(previews_host.scheme(), previews_url.scheme());
  return previews_url;
}

GURL PreviewsLitePageNavigationThrottle::GetPreviewsURL() const {
  DCHECK(!previews::IsLitePageRedirectPreviewDomain(
      navigation_handle()->GetURL()));
  return GetPreviewsURLForURL(navigation_handle()->GetURL());
}

// static
void PreviewsLitePageNavigationThrottle::LoadAndBypass(
    content::WebContents* web_contents,
    PreviewsLitePageNavigationThrottleManager* manager,
    const content::OpenURLParams& params,
    std::unique_ptr<previews::PreviewsUserData::ServerLitePageInfo> info,
    bool use_post_task) {
  DCHECK(web_contents);
  DCHECK(manager);

  manager->AddSingleBypass(params.url.spec());

  PreviewsWebContentsLifetimeHelper::CreateForWebContents(web_contents);
  PreviewsWebContentsLifetimeHelper* helper =
      PreviewsWebContentsLifetimeHelper::FromWebContents(web_contents);

  if (!use_post_task) {
    helper->PostNewNavigation(params, std::move(info));
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&PreviewsWebContentsLifetimeHelper::PostNewNavigation,
                     helper->GetWeakPtr(), params, std::move(info)));
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::TriggerPreview() const {
  net::HttpRequestHeaders request_headers;
  content::BrowserContext* browser_context =
      navigation_handle()->GetWebContents()->GetBrowserContext();

  previews::PreviewsUserData::ServerLitePageInfo* info =
      GetOrCreateServerLitePageInfo(navigation_handle(), manager_);

  // Set DRP headers.
  DataReductionProxyChromeSettings* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context);
  DCHECK(drp_settings);
  request_headers.MergeFrom(drp_settings->GetProxyRequestHeaders());

  // Set ECT header.
  request_headers.SetHeader(data_reduction_proxy::chrome_proxy_ect_header(),
                            net::GetNameForEffectiveConnectionType(
                                g_browser_process->network_quality_tracker()
                                    ->GetEffectiveConnectionType()));

  // Add in the page id to the chrome-proxy header.
  if (request_headers.HasHeader(kChromeProxyHeader)) {
    std::string header_value;
    request_headers.GetHeader(kChromeProxyHeader, &header_value);

    // 64 bit uint fits in 16 characters when represented in hexadecimal, but
    // there needs to be a trailing null terminated character in the buffer.
    char page_id_buffer[17];
    base::strings::SafeSPrintf(page_id_buffer, "%x", info->page_id);
    header_value += ", pid=" + std::string(page_id_buffer);
    request_headers.SetHeader(kChromeProxyHeader, header_value);
  }

  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  PreviewsWebContentsLifetimeHelper::CreateForWebContents(web_contents);
  PreviewsWebContentsLifetimeHelper* helper =
      PreviewsWebContentsLifetimeHelper::FromWebContents(web_contents);

  // Post a delayed task to the WebContents helper. This task will check after a
  // timeout whether the previews navigation has finished (either in success or
  // failure). If not, the helper will stop the ongoing previews navigation and
  // load the original page.
  const base::TimeDelta timeout =
      previews::params::LitePagePreviewsNavigationTimeoutDuration();
  std::unique_ptr<previews::PreviewsUserData::ServerLitePageInfo>
      timed_out_info = info->Clone();
  timed_out_info->status = previews::ServerLitePageStatus::kFailure;
  if (timeout > base::TimeDelta()) {
    base::PostDelayedTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &PreviewsWebContentsLifetimeHelper::CheckForHungNavigation,
            helper->GetWeakPtr(), GetPreviewsURL(),
            base::BindOnce(
                &PreviewsLitePageNavigationThrottle::LoadAndBypass,
                base::Unretained(web_contents), manager_,
                MakeOpenURLParams(navigation_handle(),
                                  navigation_handle()->GetURL(), std::string()),
                std::move(timed_out_info), false)),
        timeout);
  }

  // The helper class and its weak pointer protect against the WebContents
  // dying in-between the PostTask and its execution, resulting in a use after
  // free crash. Since the helper is a WebContentsUserData, it will be
  // destroyed when the WebContents is and the task will not be executed.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&PreviewsWebContentsLifetimeHelper::PostNewNavigation,
                     helper->GetWeakPtr(),
                     MakeOpenURLParams(navigation_handle(), GetPreviewsURL(),
                                       request_headers.ToString()),
                     info->Clone()));

  return content::NavigationThrottle::CANCEL;
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::MaybeNavigateToPreview() const {
  const bool trigger =
      IsEligibleForPreview() &&
      !manager_->CheckSingleBypass(navigation_handle()->GetURL().spec());
  UMA_HISTOGRAM_BOOLEAN("Previews.ServerLitePage.Triggered", trigger);
  if (trigger)
    return TriggerPreview();
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillStartRequest() {
  // Check if the user is trying to navigate to a valid previews URL in the form
  // of a reload. In such a case, cancel the navigation and start a new one to
  // the original URL instead. We might trigger this preview again, but if so
  // there will be a server_lite_page_info associated with it.
  std::string original_url;
  if (previews::ExtractOriginalURLFromLitePageRedirectURL(
          navigation_handle()->GetURL(), &original_url) &&
      navigation_handle()->GetReloadType() != content::ReloadType::NONE &&
      !GetServerLitePageInfo()) {
    // Don't use |LoadAndBypass| because we might not want to bypass.
    PreviewsWebContentsLifetimeHelper::CreateForWebContents(
        navigation_handle()->GetWebContents());
    PreviewsWebContentsLifetimeHelper* helper =
        PreviewsWebContentsLifetimeHelper::FromWebContents(
            navigation_handle()->GetWebContents());

    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &PreviewsWebContentsLifetimeHelper::PostNewNavigation,
            helper->GetWeakPtr(),
            MakeOpenURLParams(navigation_handle(), GURL(original_url),
                              std::string()),
            GetOrCreateServerLitePageInfo(navigation_handle(), manager_)
                ->Clone()));
    return content::NavigationThrottle::CANCEL;
  }

  return MaybeNavigateToPreview();
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillRedirectRequest() {
  // WillRedirectRequest is called after the navigation_handle's URL has already
  // been set to the next location. So inspect the previous URL for the presence
  // of the previews server.
  const std::vector<GURL>& redirect_chain =
      navigation_handle()->GetRedirectChain();
  // |navigation_handle()->GetURL()| is always the last element in the redirect
  // chain. So if we've come here after a redirect, the length of
  // |redirect_chain| is at least 2.
  const GURL& previous_url = redirect_chain[redirect_chain.size() - 2];

  // If we are redirecting on a preview, count some UMA and proceed.
  std::string original_url;
  if (previews::ExtractOriginalURLFromLitePageRedirectURL(previous_url,
                                                          &original_url)) {
    // A redirect means one of two things: (1) there is no preview available for
    // this page and we should redirect back to the original page. (2) the
    // previews server is forwarding along a redirect from the origin. The
    // difference between the two is where the Location header is pointing. If
    // it is pointing towards the original page, it is considered a bypass.
    // Otherwise it is just a forwarded bypass.
    if (GURL(original_url) == navigation_handle()->GetURL()) {
      SetServerLitePageInfoStatus(previews::ServerLitePageStatus::kBypass);
      manager_->AddSingleBypass(navigation_handle()->GetURL().spec());
      UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.ServerResponse",
                                ServerResponse::kPreviewUnavailable);

      // Check if the original host should be blacklisted, as directed by the
      // server.
      const net::HttpResponseHeaders* response_headers =
          navigation_handle()->GetResponseHeaders();

      std::string chrome_proxy_header;
      bool blacklist_host =
          response_headers &&
          response_headers->EnumerateHeader(nullptr, kChromeProxyHeader,
                                            &chrome_proxy_header) &&
          chrome_proxy_header.find("host-blacklisted") != std::string::npos;

      if (blacklist_host)
        manager_->BlacklistBypassedHost(GURL(original_url).host(),
                                        kBlacklistDuration);

      UMA_HISTOGRAM_BOOLEAN("Previews.ServerLitePage.HostBlacklistedOnBypass",
                            blacklist_host);

      return content::NavigationThrottle::PROCEED;
    }

    // Otherwise fall out of this if and potentially trigger again.
    UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.ServerResponse",
                              ServerResponse::kRedirect);
    SetServerLitePageInfoStatus(previews::ServerLitePageStatus::kRedirect);
  }

  return MaybeNavigateToPreview();
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillFailRequest() {
  std::string original_url;
  if (!previews::ExtractOriginalURLFromLitePageRedirectURL(
          navigation_handle()->GetURL(), &original_url)) {
    return content::NavigationThrottle::PROCEED;
  }

  UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.ServerResponse",
                            ServerResponse::kFailed);
  SetServerLitePageInfoStatus(previews::ServerLitePageStatus::kFailure);

  // The Preview was triggered but there was some irrecoverable issue (like
  // there is no network connection). Load the original page and let it go
  // through the normal process for whatever error it is.
  LoadAndBypass(
      navigation_handle()->GetWebContents(), manager_,
      MakeOpenURLParams(navigation_handle(), GURL(original_url), std::string()),
      GetOrCreateServerLitePageInfo(navigation_handle(), manager_)->Clone(),
      true);
  return content::NavigationThrottle::CANCEL;
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillProcessResponse() {
  std::string original_url;
  if (!previews::ExtractOriginalURLFromLitePageRedirectURL(
          navigation_handle()->GetURL(), &original_url)) {
    // Return early if this request was not for a Preview.
    return content::NavigationThrottle::PROCEED;
  }

  const net::HttpResponseHeaders* response_headers =
      navigation_handle()->GetResponseHeaders();

  // After this point, the given response is known to be for a Preview.
  // The Previews server will only send the following response codes: 200, 307,
  // 404, and 503. 200 and 307 should proceed as normal, 404 and 503 request the
  // client to load the original page instead because the server is not capable
  // of generating a lite page. All other response codes are treated as a 404.

  const int response_code = response_headers->response_code();

  if (response_code == net::HTTP_OK) {
    // Attempt to get the original content length and report it to Data Saver.
    const int64_t ofcl =
        data_reduction_proxy::GetDataReductionProxyOFCL(response_headers);
    if (ofcl > 0) {
      manager_->ReportDataSavings(response_headers->GetContentLength(), ofcl,
                                  GURL(original_url).host());
    }

    UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.ServerResponse",
                              ServerResponse::kOk);
    GetOrCreateServerLitePageInfo(navigation_handle(), manager_)->status =
        previews::ServerLitePageStatus::kSuccess;

    return content::NavigationThrottle::PROCEED;
  }

  if (response_code == net::HTTP_SERVICE_UNAVAILABLE) {
    std::string retry_after_header;
    base::TimeDelta retry_after = base::TimeDelta::FromSeconds(
        base::RandInt(60, previews::params::PreviewServerLoadshedMaxSeconds()));
    if (response_headers->EnumerateHeader(nullptr, "retry-after",
                                          &retry_after_header)) {
      net::HttpUtil::ParseRetryAfterHeader(retry_after_header,
                                           base::Time::Now(), &retry_after);
    }
    manager_->SetServerUnavailableFor(retry_after);

    UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.ServerResponse",
                              ServerResponse::kServiceUnavailable);
  } else if (response_code == net::HTTP_FORBIDDEN) {
    UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.ServerResponse",
                              ServerResponse::kAuthFailure);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.ServerResponse",
                              ServerResponse::kOther);
  }

  previews::PreviewsUserData::ServerLitePageInfo* info =
      GetOrCreateServerLitePageInfo(navigation_handle(), manager_);
  info->status = previews::ServerLitePageStatus::kFailure;
  LoadAndBypass(
      navigation_handle()->GetWebContents(), manager_,
      MakeOpenURLParams(navigation_handle(), GURL(original_url), std::string()),
      info->Clone(), true);
  return content::NavigationThrottle::CANCEL;
}

previews::PreviewsUserData::ServerLitePageInfo*
PreviewsLitePageNavigationThrottle::GetServerLitePageInfo() const {
  PreviewsUITabHelper* ui_tab_helper = PreviewsUITabHelper::FromWebContents(
      navigation_handle()->GetWebContents());
  if (!ui_tab_helper)
    return nullptr;

  previews::PreviewsUserData* previews_data =
      ui_tab_helper->GetPreviewsUserData(navigation_handle());
  if (!previews_data)
    return nullptr;

  return previews_data->server_lite_page_info();
}

void PreviewsLitePageNavigationThrottle::SetServerLitePageInfoStatus(
    previews::ServerLitePageStatus status) {
  previews::PreviewsUserData::ServerLitePageInfo* info =
      GetServerLitePageInfo();
  if (!info)
    return;
  info->status = status;
}

// static
previews::PreviewsUserData::ServerLitePageInfo*
PreviewsLitePageNavigationThrottle::GetOrCreateServerLitePageInfo(
    content::NavigationHandle* navigation_handle,
    PreviewsLitePageNavigationThrottleManager* manager) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(navigation_handle->GetWebContents());
  if (!ui_tab_helper)
    return nullptr;

  previews::PreviewsUserData* previews_data =
      ui_tab_helper->GetPreviewsUserData(navigation_handle);
  if (!previews_data)
    return nullptr;

  if (previews_data->server_lite_page_info()) {
    return previews_data->server_lite_page_info();
  }

  previews_data->set_server_lite_page_info(
      std::make_unique<previews::PreviewsUserData::ServerLitePageInfo>());

  DataReductionProxyChromeSettings* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
  base::Optional<std::string> session_id;
  if (drp_settings) {
    session_id = data_reduction_proxy::DataReductionProxyRequestOptions::
        GetSessionKeyFromRequestHeaders(drp_settings->GetProxyRequestHeaders());
  }

  previews::PreviewsUserData::ServerLitePageInfo* info =
      previews_data->server_lite_page_info();
  info->original_navigation_start = navigation_handle->NavigationStart();
  if (session_id.has_value())
    info->drp_session_key = session_id.value();

  const ChromeNavigationUIData* chrome_navigation_ui_data =
      static_cast<const ChromeNavigationUIData*>(
          navigation_handle->GetNavigationUIData());
  if (chrome_navigation_ui_data)
    info->page_id = chrome_navigation_ui_data->data_reduction_proxy_page_id();
  // The page id may not be set in some corner cases (like forward navigation),
  // so make sure it gets set here.
  if (info->page_id == 0U)
    info->page_id = manager->GeneratePageID();

  return info;
}

const char* PreviewsLitePageNavigationThrottle::GetNameForLogging() {
  return "PreviewsLitePageNavigationThrottle";
}
