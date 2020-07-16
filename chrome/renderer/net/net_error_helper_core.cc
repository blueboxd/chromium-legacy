// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper_core.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/error_page/common/error_page_params.h"
#include "components/error_page/common/localized_error.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_thread.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace {

base::TimeDelta GetAutoReloadTime(size_t reload_count) {
  static const int kDelaysMs[] = {0,      5000,   30000,  60000,
                                  300000, 600000, 1800000};
  if (reload_count >= base::size(kDelaysMs))
    reload_count = base::size(kDelaysMs) - 1;
  return base::TimeDelta::FromMilliseconds(kDelaysMs[reload_count]);
}

// Returns whether |error| is a DNS-related error (and therefore whether
// the tab helper should start a DNS probe after receiving it).
bool IsNetDnsError(const error_page::Error& error) {
  return error.domain() == error_page::Error::kNetErrorDomain &&
         net::IsHostnameResolutionError(error.reason());
}

}  // namespace

struct NetErrorHelperCore::ErrorPageInfo {
  ErrorPageInfo(error_page::Error error, bool was_failed_post)
      : error(error),
        was_failed_post(was_failed_post),
        needs_dns_updates(false),
        is_finished_loading(false),
        auto_reload_triggered(false) {}

  // Information about the failed page load.
  error_page::Error error;
  bool was_failed_post;

  // Information about the status of the error page.

  // True if a page is a DNS error page and has not yet received a final DNS
  // probe status.
  bool needs_dns_updates;
  bool dns_probe_complete = false;

  // True if a page has completed loading, at which point it can receive
  // updates.
  bool is_finished_loading;

  // True if the auto-reload timer has fired and a reload is or has been in
  // flight.
  bool auto_reload_triggered;

  error_page::LocalizedError::PageState page_state;
};

bool NetErrorHelperCore::IsReloadableError(
    const NetErrorHelperCore::ErrorPageInfo& info) {
  GURL url = info.error.url();
  return info.error.domain() == error_page::Error::kNetErrorDomain &&
         info.error.reason() != net::ERR_ABORTED &&
         // For now, net::ERR_UNKNOWN_URL_SCHEME is only being displayed on
         // Chrome for Android.
         info.error.reason() != net::ERR_UNKNOWN_URL_SCHEME &&
         // Do not trigger if the server rejects a client certificate.
         // https://crbug.com/431387
         !net::IsClientCertificateError(info.error.reason()) &&
         // Some servers reject client certificates with a generic
         // handshake_failure alert.
         // https://crbug.com/431387
         info.error.reason() != net::ERR_SSL_PROTOCOL_ERROR &&
         // Do not trigger for blacklisted URLs.
         // https://crbug.com/803839
         // Do not trigger for requests that were blocked by the browser itself.
         !net::IsRequestBlockedError(info.error.reason()) &&
         !info.was_failed_post &&
         // Do not trigger for this error code because it is used by Chrome
         // while an auth prompt is being displayed.
         info.error.reason() != net::ERR_INVALID_AUTH_CREDENTIALS &&
         // Don't auto-reload non-http/https schemas.
         // https://crbug.com/471713
         url.SchemeIsHTTPOrHTTPS() &&
         // Don't auto reload if the error was a secure DNS network error, since
         // the reload may interfere with the captive portal probe state.
         // TODO(crbug.com/1016164): Explore how to allow reloads for secure DNS
         // network errors without interfering with the captive portal probe
         // state.
         !info.error.resolve_error_info().is_secure_network_error;
}

NetErrorHelperCore::NetErrorHelperCore(Delegate* delegate,
                                       bool auto_reload_enabled,
                                       bool is_visible)
    : delegate_(delegate),
      last_probe_status_(error_page::DNS_PROBE_POSSIBLE),
      can_show_network_diagnostics_dialog_(false),
      auto_reload_enabled_(auto_reload_enabled),
      auto_reload_timer_(new base::OneShotTimer()),
      auto_reload_paused_(false),
      auto_reload_in_flight_(false),
      uncommitted_load_started_(false),
      online_(content::RenderThread::Get()->IsOnline()),
      visible_(is_visible),
      auto_reload_count_(0),
      navigation_from_button_(NO_BUTTON),
      custom_error_page_(false)
#if defined(OS_ANDROID)
      ,
      page_auto_fetcher_helper_(
          std::make_unique<PageAutoFetcherHelper>(delegate->GetRenderFrame()))
#endif
{
}

NetErrorHelperCore::~NetErrorHelperCore() = default;

void NetErrorHelperCore::CancelPendingAutoReload() {
  auto_reload_timer_->Stop();
  auto_reload_paused_ = false;
}

void NetErrorHelperCore::OnStop() {
  CancelPendingAutoReload();
  uncommitted_load_started_ = false;
  auto_reload_count_ = 0;
  auto_reload_in_flight_ = false;
}

void NetErrorHelperCore::OnWasShown() {
  visible_ = true;
  if (auto_reload_paused_)
    MaybeStartAutoReloadTimer();
}

void NetErrorHelperCore::OnWasHidden() {
  visible_ = false;
  PauseAutoReloadTimer();
}

void NetErrorHelperCore::OnStartLoad(FrameType frame_type, PageType page_type) {
  if (frame_type != MAIN_FRAME)
    return;

  uncommitted_load_started_ = true;

  // If there's no pending error page information associated with the page load,
  // or the new page is not an error page, then reset pending error page state.
  if (!pending_error_page_info_ || page_type != ERROR_PAGE) {
    CancelPendingAutoReload();
  } else {
    // Halt auto-reload if it's currently scheduled. OnFinishLoad will trigger
    // auto-reload if appropriate.
    PauseAutoReloadTimer();
  }
}

void NetErrorHelperCore::OnCommitLoad(FrameType frame_type, const GURL& url) {
  if (frame_type != MAIN_FRAME)
    return;

  // If a page is committing, either it's an error page and autoreload will be
  // started again below, or it's a success page and we need to clear autoreload
  // state.
  auto_reload_in_flight_ = false;

  // uncommitted_load_started_ could already be false, since RenderFrameImpl
  // calls OnCommitLoad once for each in-page navigation (like a fragment
  // change) with no corresponding OnStartLoad.
  uncommitted_load_started_ = false;

#if defined(OS_ANDROID)
  // Don't need this state. It will be refreshed if another error page is
  // loaded.
  available_content_helper_.Reset();
  page_auto_fetcher_helper_->OnCommitLoad();
#endif

  // Track if an error occurred due to a page button press.
  // This isn't perfect; if (for instance), the server is slow responding
  // to a request generated from the page reload button, and the user hits
  // the browser reload button, this code will still believe the
  // result is from the page reload button.
  if (committed_error_page_info_ && pending_error_page_info_ &&
      navigation_from_button_ != NO_BUTTON &&
      committed_error_page_info_->error.url() ==
          pending_error_page_info_->error.url()) {
    DCHECK(navigation_from_button_ == RELOAD_BUTTON);
    RecordEvent(error_page::NETWORK_ERROR_PAGE_RELOAD_BUTTON_ERROR);
  }
  navigation_from_button_ = NO_BUTTON;

  committed_error_page_info_ = std::move(pending_error_page_info_);
}

void NetErrorHelperCore::ErrorPageLoadedWithFinalErrorCode() {
  ErrorPageInfo* page_info = committed_error_page_info_.get();
  DCHECK(page_info);
  error_page::Error updated_error = GetUpdatedError(*page_info);

  if (page_info->page_state.is_offline_error)
    RecordEvent(error_page::NETWORK_ERROR_PAGE_OFFLINE_ERROR_SHOWN);

#if defined(OS_ANDROID)
  // The fetch functions shouldn't be triggered multiple times per page load.
  if (page_info->page_state.offline_content_feature_enabled) {
    available_content_helper_.FetchAvailableContent(base::BindOnce(
        &Delegate::OfflineContentAvailable, base::Unretained(delegate_)));
  }

  // |TrySchedule()| shouldn't be called more than once per page.
  if (page_info->page_state.auto_fetch_allowed) {
    page_auto_fetcher_helper_->TrySchedule(
        false, base::BindOnce(&Delegate::SetAutoFetchState,
                              base::Unretained(delegate_)));
  }
#endif  // defined(OS_ANDROID)

  if (page_info->page_state.download_button_shown)
    RecordEvent(error_page::NETWORK_ERROR_PAGE_DOWNLOAD_BUTTON_SHOWN);

  if (page_info->page_state.reload_button_shown)
    RecordEvent(error_page::NETWORK_ERROR_PAGE_RELOAD_BUTTON_SHOWN);

  delegate_->SetIsShowingDownloadButton(
      page_info->page_state.download_button_shown);
}

void NetErrorHelperCore::OnFinishLoad(FrameType frame_type) {
  if (frame_type != MAIN_FRAME)
    return;

  if (!committed_error_page_info_) {
    auto_reload_count_ = 0;
    return;
  }
  committed_error_page_info_->is_finished_loading = true;

  RecordEvent(error_page::NETWORK_ERROR_PAGE_SHOWN);

  delegate_->SetIsShowingDownloadButton(
      committed_error_page_info_->page_state.download_button_shown);

  delegate_->EnablePageHelperFunctions();

  if (auto_reload_enabled_ && IsReloadableError(*committed_error_page_info_) &&
      !custom_error_page_) {
    MaybeStartAutoReloadTimer();
  }

  DVLOG(1) << "Error page finished loading; sending saved status.";
  if (committed_error_page_info_->needs_dns_updates) {
    if (last_probe_status_ != error_page::DNS_PROBE_POSSIBLE)
      UpdateErrorPage();
  } else {
    ErrorPageLoadedWithFinalErrorCode();
  }
}

void NetErrorHelperCore::PrepareErrorPage(FrameType frame_type,
                                          const error_page::Error& error,
                                          bool is_failed_post,
                                          std::string* error_html) {
  if (frame_type == MAIN_FRAME) {
    pending_error_page_info_.reset(new ErrorPageInfo(error, is_failed_post));
    PrepareErrorPageForMainFrame(pending_error_page_info_.get(), error_html);
  } else {
    if (error_html) {
      custom_error_page_ = false;
      delegate_->GenerateLocalizedErrorPage(
          error, is_failed_post,
          false /* No diagnostics dialogs allowed for subframes. */, nullptr,
          error_html);
    } else {
      custom_error_page_ = true;
    }
  }
}

void NetErrorHelperCore::OnNetErrorInfo(error_page::DnsProbeStatus status) {
  DCHECK_NE(error_page::DNS_PROBE_POSSIBLE, status);

  last_probe_status_ = status;

  if (!committed_error_page_info_ ||
      !committed_error_page_info_->needs_dns_updates ||
      !committed_error_page_info_->is_finished_loading) {
    return;
  }

  UpdateErrorPage();
}

void NetErrorHelperCore::OnSetCanShowNetworkDiagnosticsDialog(
    bool can_show_network_diagnostics_dialog) {
  can_show_network_diagnostics_dialog_ = can_show_network_diagnostics_dialog;
}

void NetErrorHelperCore::OnEasterEggHighScoreReceived(int high_score) {
  if (!committed_error_page_info_ ||
      !committed_error_page_info_->is_finished_loading) {
    return;
  }

  delegate_->InitializeErrorPageEasterEggHighScore(high_score);
}

void NetErrorHelperCore::PrepareErrorPageForMainFrame(
    ErrorPageInfo* pending_error_page_info,
    std::string* error_html) {
  std::string error_param;
  error_page::Error error = pending_error_page_info->error;

  if (IsNetDnsError(pending_error_page_info->error)) {
    // The last probe status needs to be reset if this is a DNS error.  This
    // means that if a DNS error page is committed but has not yet finished
    // loading, a DNS probe status scheduled to be sent to it may be thrown
    // out, but since the new error page should trigger a new DNS probe, it
    // will just get the results for the next page load.
    last_probe_status_ = error_page::DNS_PROBE_POSSIBLE;
    pending_error_page_info->needs_dns_updates = true;
    error = GetUpdatedError(*pending_error_page_info);
  }
  if (error_html) {
    custom_error_page_ = false;
    pending_error_page_info->page_state = delegate_->GenerateLocalizedErrorPage(
        error, pending_error_page_info->was_failed_post,
        can_show_network_diagnostics_dialog_, nullptr, error_html);
  } else {
    custom_error_page_ = true;
  }
}

void NetErrorHelperCore::UpdateErrorPage() {
  DCHECK(committed_error_page_info_->needs_dns_updates);
  DCHECK(committed_error_page_info_->is_finished_loading);
  DCHECK_NE(error_page::DNS_PROBE_POSSIBLE, last_probe_status_);

  UMA_HISTOGRAM_ENUMERATION("DnsProbe.ErrorPageUpdateStatus",
                            last_probe_status_, error_page::DNS_PROBE_MAX);
  // Every status other than error_page::DNS_PROBE_POSSIBLE and
  // error_page::DNS_PROBE_STARTED is a final status code.  Once one is reached,
  // the page does not need further updates.
  if (last_probe_status_ != error_page::DNS_PROBE_STARTED) {
    committed_error_page_info_->needs_dns_updates = false;
    committed_error_page_info_->dns_probe_complete = true;
  }

  error_page::LocalizedError::PageState new_state =
      delegate_->UpdateErrorPage(GetUpdatedError(*committed_error_page_info_),
                                 committed_error_page_info_->was_failed_post,
                                 can_show_network_diagnostics_dialog_);

  committed_error_page_info_->page_state = std::move(new_state);
  if (!committed_error_page_info_->needs_dns_updates)
    ErrorPageLoadedWithFinalErrorCode();
}

error_page::Error NetErrorHelperCore::GetUpdatedError(
    const ErrorPageInfo& error_info) const {
  // If a probe didn't run or wasn't conclusive, restore the original error.
  const bool dns_probe_used =
      error_info.needs_dns_updates || error_info.dns_probe_complete;
  if (!dns_probe_used || last_probe_status_ == error_page::DNS_PROBE_NOT_RUN ||
      last_probe_status_ == error_page::DNS_PROBE_FINISHED_INCONCLUSIVE) {
    return error_info.error;
  }

  return error_page::Error::DnsProbeError(
      error_info.error.url(), last_probe_status_,
      error_info.error.stale_copy_in_cache());
}

void NetErrorHelperCore::Reload() {
  if (!committed_error_page_info_)
    return;
  delegate_->ReloadFrame();
}

bool NetErrorHelperCore::MaybeStartAutoReloadTimer() {
  // Automation tools expect to be in control of reloads.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutomation)) {
    return false;
  }

  if (!committed_error_page_info_ ||
      !committed_error_page_info_->is_finished_loading ||
      pending_error_page_info_ || uncommitted_load_started_) {
    return false;
  }

  StartAutoReloadTimer();
  return true;
}

void NetErrorHelperCore::StartAutoReloadTimer() {
  DCHECK(committed_error_page_info_);
  DCHECK(IsReloadableError(*committed_error_page_info_));

  committed_error_page_info_->auto_reload_triggered = true;

  if (!online_ || !visible_) {
    auto_reload_paused_ = true;
    return;
  }

  auto_reload_paused_ = false;
  base::TimeDelta delay = GetAutoReloadTime(auto_reload_count_);
  auto_reload_timer_->Stop();
  auto_reload_timer_->Start(
      FROM_HERE, delay,
      base::BindOnce(&NetErrorHelperCore::AutoReloadTimerFired,
                     base::Unretained(this)));
}

void NetErrorHelperCore::AutoReloadTimerFired() {
  // AutoReloadTimerFired only runs if:
  // 1. StartAutoReloadTimer was previously called, which requires that
  //    committed_error_page_info_ is populated;
  // 2. No other page load has started since (1), since OnStartLoad stops the
  //    auto-reload timer.
  DCHECK(committed_error_page_info_);

  auto_reload_count_++;
  auto_reload_in_flight_ = true;
  Reload();
}

void NetErrorHelperCore::PauseAutoReloadTimer() {
  if (!auto_reload_timer_->IsRunning())
    return;
  DCHECK(committed_error_page_info_);
  DCHECK(!auto_reload_paused_);
  DCHECK(committed_error_page_info_->auto_reload_triggered);
  auto_reload_timer_->Stop();
  auto_reload_paused_ = true;
}

void NetErrorHelperCore::NetworkStateChanged(bool online) {
  bool was_online = online_;
  online_ = online;
  if (!was_online && online) {
    // Transitioning offline -> online
    if (auto_reload_paused_)
      MaybeStartAutoReloadTimer();
  } else if (was_online && !online) {
    // Transitioning online -> offline
    if (auto_reload_timer_->IsRunning())
      auto_reload_count_ = 0;
    PauseAutoReloadTimer();
  }
}

bool NetErrorHelperCore::ShouldSuppressErrorPage(FrameType frame_type,
                                                 const GURL& url,
                                                 int error_code) {
  // Don't suppress child frame errors.
  if (frame_type != MAIN_FRAME)
    return false;

  // If there's no auto reload attempt in flight, this error page didn't come
  // from auto reload, so don't suppress it.
  if (!auto_reload_in_flight_)
    return false;

  // Even with auto_reload_in_flight_ error page may not come from
  // the auto reload when proceeding from error CERT_AUTHORITY_INVALID
  // to error INVALID_AUTH_CREDENTIALS, so do not suppress the error page
  // for the new error code.
  if (committed_error_page_info_ &&
      committed_error_page_info_->error.reason() != error_code)
    return false;

  uncommitted_load_started_ = false;
  // This serves to terminate the auto-reload in flight attempt. If
  // ShouldSuppressErrorPage is called, the auto-reload yielded an error, which
  // means the request was already sent.
  auto_reload_in_flight_ = false;
  MaybeStartAutoReloadTimer();
  return true;
}

#if defined(OS_ANDROID)
void NetErrorHelperCore::SetPageAutoFetcherHelperForTesting(
    std::unique_ptr<PageAutoFetcherHelper> page_auto_fetcher_helper) {
  page_auto_fetcher_helper_ = std::move(page_auto_fetcher_helper);
}
#endif

void NetErrorHelperCore::ExecuteButtonPress(Button button) {
  // If there's no committed error page, should not be invoked.
  DCHECK(committed_error_page_info_);

  switch (button) {
    case RELOAD_BUTTON:
      RecordEvent(error_page::NETWORK_ERROR_PAGE_RELOAD_BUTTON_CLICKED);
      navigation_from_button_ = RELOAD_BUTTON;
      Reload();
      return;
    case MORE_BUTTON:
      // Visual effects on page are handled in Javascript code.
      RecordEvent(error_page::NETWORK_ERROR_PAGE_MORE_BUTTON_CLICKED);
      return;
    case EASTER_EGG:
      RecordEvent(error_page::NETWORK_ERROR_EASTER_EGG_ACTIVATED);
      delegate_->RequestEasterEggHighScore();
      return;
    case DIAGNOSE_ERROR:
      RecordEvent(error_page::NETWORK_ERROR_DIAGNOSE_BUTTON_CLICKED);
      delegate_->DiagnoseError(committed_error_page_info_->error.url());
      return;
    case DOWNLOAD_BUTTON:
      RecordEvent(error_page::NETWORK_ERROR_PAGE_DOWNLOAD_BUTTON_CLICKED);
      delegate_->DownloadPageLater();
      return;
    case NO_BUTTON:
      NOTREACHED();
      return;
  }
}

void NetErrorHelperCore::LaunchOfflineItem(const std::string& id,
                                           const std::string& name_space) {
#if defined(OS_ANDROID)
  available_content_helper_.LaunchItem(id, name_space);
#endif
}

void NetErrorHelperCore::LaunchDownloadsPage() {
#if defined(OS_ANDROID)
  available_content_helper_.LaunchDownloadsPage();
#endif
}

void NetErrorHelperCore::SavePageForLater() {
#if defined(OS_ANDROID)
  page_auto_fetcher_helper_->TrySchedule(
      /*user_requested=*/true, base::BindOnce(&Delegate::SetAutoFetchState,
                                              base::Unretained(delegate_)));
#endif
}

void NetErrorHelperCore::CancelSavePage() {
#if defined(OS_ANDROID)
  page_auto_fetcher_helper_->CancelSchedule();
#endif
}

void NetErrorHelperCore::ListVisibilityChanged(bool is_visible) {
#if defined(OS_ANDROID)
  available_content_helper_.ListVisibilityChanged(is_visible);
#endif
}
