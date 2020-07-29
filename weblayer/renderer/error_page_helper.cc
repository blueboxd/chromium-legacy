// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/renderer/error_page_helper.h"

#include "base/command_line.h"
#include "components/error_page/common/error.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "weblayer/common/features.h"

namespace weblayer {

struct ErrorPageHelper::ErrorPageInfo {
  explicit ErrorPageInfo(const error_page::Error& error) : error(error) {}

  // Information about the failed page load.
  error_page::Error error;

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

void ErrorPageHelper::PrepareErrorPage(const error_page::Error& error) {
  pending_error_page_info_ = std::make_unique<ErrorPageInfo>(error);
}

void ErrorPageHelper::DidCommitProvisionalLoad(ui::PageTransition transition) {
  committed_error_page_info_ = std::move(pending_error_page_info_);
  weak_factory_.InvalidateWeakPtrs();
}

void ErrorPageHelper::DidFinishLoad() {
  if (!committed_error_page_info_)
    return;

  security_interstitials::SecurityInterstitialPageController::Install(
      render_frame(), weak_factory_.GetWeakPtr());

  committed_error_page_info_->is_finished_loading = true;
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
      // Used when user opts in to extended safe browsing
      interface->DoReport();
      break;
    case security_interstitials::CMD_DONT_REPORT:
      interface->DontReport();
      break;
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY:
      interface->OpenReportingPrivacy();
      break;
    case security_interstitials::CMD_OPEN_WHITEPAPER:
      interface->OpenWhitepaper();
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
      RenderFrameObserverTracker<ErrorPageHelper>(render_frame) {}

ErrorPageHelper::~ErrorPageHelper() = default;

void ErrorPageHelper::Reload() {
  if (!committed_error_page_info_)
    return;
  render_frame()->GetWebFrame()->StartReload(blink::WebFrameLoadType::kReload);
}

}  // namespace weblayer
