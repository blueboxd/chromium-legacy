// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/renderer/error_page_helper.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace weblayer {

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

void ErrorPageHelper::PrepareErrorPage() {
  next_load_is_error_ = true;
}

void ErrorPageHelper::DidCommitProvisionalLoad(bool is_same_document_navigation,
                                               ui::PageTransition transition) {
  if (is_same_document_navigation)
    return;

  weak_factory_.InvalidateWeakPtrs();
  this_load_is_error_ = next_load_is_error_;
  next_load_is_error_ = false;
}

void ErrorPageHelper::DidFinishLoad() {
  if (this_load_is_error_) {
    security_interstitials::SecurityInterstitialPageController::Install(
        render_frame(), weak_factory_.GetWeakPtr());
  }
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
    case security_interstitials::CMD_RELOAD:
      interface->Reload();
      break;
    case security_interstitials::CMD_OPEN_LOGIN:
      interface->OpenLogin();
      break;
    case security_interstitials::CMD_OPEN_DIAGNOSTIC:
    case security_interstitials::CMD_OPEN_DATE_SETTINGS:
    case security_interstitials::CMD_DO_REPORT:
    case security_interstitials::CMD_DONT_REPORT:
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY:
    case security_interstitials::CMD_OPEN_WHITEPAPER:
    case security_interstitials::CMD_REPORT_PHISHING_ERROR:
      // Commands not used by the generic SSL error page.
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
      RenderFrameObserverTracker<ErrorPageHelper>(render_frame) {}

ErrorPageHelper::~ErrorPageHelper() = default;

}  // namespace weblayer
