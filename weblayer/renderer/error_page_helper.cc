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
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
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
  if (is_disabled_for_next_error_) {
    is_disabled_for_next_error_ = false;
    return;
  }
  pending_error_page_info_ = std::make_unique<ErrorPageInfo>(error);
}

void ErrorPageHelper::DidCommitProvisionalLoad(ui::PageTransition transition) {
  is_disabled_for_next_error_ = false;
  committed_error_page_info_ = std::move(pending_error_page_info_);
}

void ErrorPageHelper::DidFinishLoad() {
  if (!committed_error_page_info_)
    return;

  security_interstitials::SecurityInterstitialPageController::Install(
      render_frame());

  committed_error_page_info_->is_finished_loading = true;
}

void ErrorPageHelper::OnDestruct() {
  delete this;
}


void ErrorPageHelper::DisableErrorPageHelperForNextError() {
  is_disabled_for_next_error_ = true;
}

ErrorPageHelper::ErrorPageHelper(content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<ErrorPageHelper>(render_frame) {
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&ErrorPageHelper::BindErrorPageHelper,
                          weak_factory_.GetWeakPtr()));
}

ErrorPageHelper::~ErrorPageHelper() = default;

void ErrorPageHelper::Reload() {
  if (!committed_error_page_info_)
    return;
  render_frame()->GetWebFrame()->StartReload(blink::WebFrameLoadType::kReload);
}

void ErrorPageHelper::BindErrorPageHelper(
    mojo::PendingAssociatedReceiver<mojom::ErrorPageHelper> receiver) {
  // There is only a need for a single receiver to be bound at a time.
  error_page_helper_receiver_.reset();
  error_page_helper_receiver_.Bind(std::move(receiver));
}

}  // namespace weblayer
