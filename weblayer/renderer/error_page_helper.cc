// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/renderer/error_page_helper.h"

#include "components/error_page/common/error.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "weblayer/common/features.h"

namespace weblayer {
namespace {

base::TimeDelta GetAutoReloadTime(size_t reload_count) {
  static const int kDelaysMs[] = {0,      5000,   30000,  60000,
                                  300000, 600000, 1800000};
  if (reload_count >= base::size(kDelaysMs))
    reload_count = base::size(kDelaysMs) - 1;
  return base::TimeDelta::FromMilliseconds(kDelaysMs[reload_count]);
}

bool IsErrorPage(const GURL& url) {
  return url.is_valid() && url.spec() == content::kUnreachableWebDataURL;
}

bool IsReloadableError(const error_page::Error& error, bool was_failed_post) {
  const GURL& url = error.url();
  return error.domain() == error_page::Error::kNetErrorDomain &&
         error.reason() != net::ERR_ABORTED &&
         // For now, net::ERR_UNKNOWN_URL_SCHEME is only being displayed on
         // Chrome for Android.
         error.reason() != net::ERR_UNKNOWN_URL_SCHEME &&
         // Do not trigger if the server rejects a client certificate.
         // https://crbug.com/431387
         !net::IsClientCertificateError(error.reason()) &&
         // Some servers reject client certificates with a generic
         // handshake_failure alert.
         // https://crbug.com/431387
         error.reason() != net::ERR_SSL_PROTOCOL_ERROR &&
         // Do not trigger for blacklisted URLs.
         // https://crbug.com/803839
         error.reason() != net::ERR_BLOCKED_BY_ADMINISTRATOR &&
         // Do not trigger for requests that were blocked by the browser itself.
         error.reason() != net::ERR_BLOCKED_BY_CLIENT && !was_failed_post &&
         // Do not trigger for this error code because it is used by Chrome
         // while an auth prompt is being displayed.
         error.reason() != net::ERR_INVALID_AUTH_CREDENTIALS &&
         // Don't auto-reload non-http/https schemas.
         // https://crbug.com/471713
         url.SchemeIsHTTPOrHTTPS() &&
         // Don't auto reload if the error was a secure DNS network error, since
         // the reload may interfere with the captive portal probe state.
         // TODO(crbug.com/1016164): Explore how to allow reloads for secure DNS
         // network errors without interfering with the captive portal probe
         // state.
         !error.resolve_error_info().is_secure_network_error;
}

}  // namespace

struct ErrorPageHelper::ErrorPageInfo {
  ErrorPageInfo(const error_page::Error& error, bool was_failed_post)
      : error(error), was_failed_post(was_failed_post) {}

  // Information about the failed page load.
  error_page::Error error;
  bool was_failed_post = false;

  // True if a page has completed loading, at which point it can receive
  // updates.
  bool is_finished_loading = false;
};

// static
void ErrorPageHelper::Create(content::RenderFrame* render_frame) {
  if (render_frame->IsMainFrame())
    new ErrorPageHelper(render_frame);
}

// static
ErrorPageHelper* ErrorPageHelper::GetForFrame(
    content::RenderFrame* render_frame) {
  return render_frame->IsMainFrame() ? Get(render_frame) : nullptr;
}

void ErrorPageHelper::PrepareErrorPage(const error_page::Error& error,
                                       bool was_failed_post) {
  pending_error_page_info_ =
      std::make_unique<ErrorPageInfo>(error, was_failed_post);
}

bool ErrorPageHelper::ShouldSuppressErrorPage() {
  // If there's no auto reload attempt in flight, this error page didn't come
  // from auto reload, so don't suppress it.
  if (!auto_reload_in_flight_)
    return false;

  uncommitted_load_started_ = false;
  // This serves to terminate the auto-reload in flight attempt. If
  // ShouldSuppressErrorPage is called, the auto-reload yielded an error, which
  // means the request was already sent.
  auto_reload_in_flight_ = false;
  MaybeStartAutoReloadTimer();
  return true;
}

void ErrorPageHelper::DidStartNavigation(
    const GURL& url,
    base::Optional<blink::WebNavigationType> navigation_type) {
  uncommitted_load_started_ = true;

  // If there's no pending error page information associated with the page load,
  // or the new page is not an error page, then reset pending error page state.
  if (!pending_error_page_info_ || !IsErrorPage(url)) {
    CancelPendingReload();
  } else {
    // Halt auto-reload if it's currently scheduled. OnFinishLoad will trigger
    // auto-reload if appropriate.
    PauseAutoReloadTimer();
  }
}

void ErrorPageHelper::DidCommitProvisionalLoad(bool is_same_document_navigation,
                                               ui::PageTransition transition) {
  if (is_same_document_navigation)
    return;

  // If a page is committing, either it's an error page and autoreload will be
  // started again below, or it's a success page and we need to clear autoreload
  // state.
  auto_reload_in_flight_ = false;

  // uncommitted_load_started_ could already be false, since RenderFrameImpl
  // calls OnCommitLoad once for each in-page navigation (like a fragment
  // change) with no corresponding OnStartLoad.
  uncommitted_load_started_ = false;

  committed_error_page_info_ = std::move(pending_error_page_info_);

  weak_factory_.InvalidateWeakPtrs();
}

void ErrorPageHelper::DidFinishLoad() {
  if (!committed_error_page_info_) {
    auto_reload_count_ = 0;
    return;
  }

  security_interstitials::SecurityInterstitialPageController::Install(
      render_frame(), weak_factory_.GetWeakPtr());

  committed_error_page_info_->is_finished_loading = true;
  if (IsReloadableError(committed_error_page_info_->error,
                        committed_error_page_info_->was_failed_post)) {
    MaybeStartAutoReloadTimer();
  }
}

void ErrorPageHelper::OnStop() {
  CancelPendingReload();
  uncommitted_load_started_ = false;
  auto_reload_count_ = 0;
  auto_reload_in_flight_ = false;
}

void ErrorPageHelper::WasShown() {
  if (auto_reload_paused_)
    MaybeStartAutoReloadTimer();
}

void ErrorPageHelper::WasHidden() {
  PauseAutoReloadTimer();
}

void ErrorPageHelper::OnDestruct() {
  delete this;
}

void ErrorPageHelper::SendCommand(
    security_interstitials::SecurityInterstitialCommand command) {
  mojo::AssociatedRemote<security_interstitials::mojom::InterstitialCommands>
      interface = GetInterface();
  switch (command) {
    case security_interstitials::CMD_DONT_PROCEED:
      interface->DontProceed();
      break;
    case security_interstitials::CMD_PROCEED:
      interface->Proceed();
      break;
    case security_interstitials::CMD_SHOW_MORE_SECTION:
      interface->ShowMoreSection();
      break;
    case security_interstitials::CMD_OPEN_HELP_CENTER:
      interface->OpenHelpCenter();
      break;
    case security_interstitials::CMD_OPEN_DIAGNOSTIC:
      // Used by safebrowsing interstials.
      interface->OpenDiagnostic();
      break;
    case security_interstitials::CMD_RELOAD:
      interface->Reload();
      break;
    case security_interstitials::CMD_OPEN_LOGIN:
      interface->OpenLogin();
      break;
    case security_interstitials::CMD_OPEN_DATE_SETTINGS:
      interface->OpenDateSettings();
      break;
    case security_interstitials::CMD_REPORT_PHISHING_ERROR:
      // Used by safebrowsing phishing interstitial.
      interface->ReportPhishingError();
      break;
    case security_interstitials::CMD_DO_REPORT:
    case security_interstitials::CMD_DONT_REPORT:
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY:
    case security_interstitials::CMD_OPEN_WHITEPAPER:
      // Commands not used by the generic SSL error pages.
      // Also not currently used by the safebrowsing error pages.
      NOTREACHED();
      break;
    case security_interstitials::CMD_ERROR:
    case security_interstitials::CMD_TEXT_FOUND:
    case security_interstitials::CMD_TEXT_NOT_FOUND:
      // Commands for testing.
      NOTREACHED();
      break;
  }
}

mojo::AssociatedRemote<security_interstitials::mojom::InterstitialCommands>
ErrorPageHelper::GetInterface() {
  mojo::AssociatedRemote<security_interstitials::mojom::InterstitialCommands>
      interface;
  render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&interface);
  return interface;
}

ErrorPageHelper::ErrorPageHelper(content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<ErrorPageHelper>(render_frame),
      online_(content::RenderThread::Get()->IsOnline()) {
  content::RenderThread::Get()->AddObserver(this);
}

ErrorPageHelper::~ErrorPageHelper() {
  content::RenderThread::Get()->RemoveObserver(this);
}

void ErrorPageHelper::Reload() {
  if (!committed_error_page_info_)
    return;
  render_frame()->GetWebFrame()->StartReload(blink::WebFrameLoadType::kReload);
}

void ErrorPageHelper::MaybeStartAutoReloadTimer() {
  // Automation tools expect to be in control of reloads.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutomation) ||
      !base::FeatureList::IsEnabled(features::kEnableAutoReload)) {
    return;
  }

  if (!committed_error_page_info_ ||
      !committed_error_page_info_->is_finished_loading ||
      pending_error_page_info_ || uncommitted_load_started_) {
    return;
  }

  StartAutoReloadTimer();
}

void ErrorPageHelper::StartAutoReloadTimer() {
  DCHECK(committed_error_page_info_);
  DCHECK(IsReloadableError(committed_error_page_info_->error,
                           committed_error_page_info_->was_failed_post));

  if (!online_ || render_frame()->IsHidden()) {
    auto_reload_paused_ = true;
    return;
  }

  auto_reload_paused_ = false;
  base::TimeDelta delay = GetAutoReloadTime(auto_reload_count_);
  auto_reload_timer_.Stop();
  auto_reload_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&ErrorPageHelper::AutoReloadTimerFired,
                     base::Unretained(this)));
}

void ErrorPageHelper::AutoReloadTimerFired() {
  // AutoReloadTimerFired only runs if:
  // 1. StartAutoReloadTimer was previously called, which requires that
  //    committed_error_page_info_ is populated;
  // 2. No other page load has started since (1), since DidStartNavigation stops
  //    the auto-reload timer.
  DCHECK(committed_error_page_info_);

  auto_reload_count_++;
  auto_reload_in_flight_ = true;
  Reload();
}

void ErrorPageHelper::PauseAutoReloadTimer() {
  if (!auto_reload_timer_.IsRunning())
    return;
  DCHECK(committed_error_page_info_);
  DCHECK(!auto_reload_paused_);
  auto_reload_timer_.Stop();
  auto_reload_paused_ = true;
}

void ErrorPageHelper::NetworkStateChanged(bool online) {
  bool was_online = online_;
  online_ = online;
  if (!was_online && online) {
    // Transitioning offline -> online
    if (auto_reload_paused_)
      MaybeStartAutoReloadTimer();
  } else if (was_online && !online) {
    // Transitioning online -> offline
    if (auto_reload_timer_.IsRunning())
      auto_reload_count_ = 0;
    PauseAutoReloadTimer();
  }
}

void ErrorPageHelper::CancelPendingReload() {
  auto_reload_timer_.Stop();
  auto_reload_paused_ = false;
}

}  // namespace weblayer
