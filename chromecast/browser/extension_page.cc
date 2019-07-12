// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extension_page.h"

#include <utility>

#include "base/logging.h"
#include "chromecast/browser/cast_content_window_aura.h"
#include "chromecast/browser/cast_extension_host.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/aura/window.h"

namespace chromecast {

ExtensionPage::ExtensionPage(
    const CastWebContents::InitParams& init_params,
    const shell::CastContentWindow::CreateParams& window_params,
    std::unique_ptr<CastExtensionHost> extension_host,
    CastWindowManager* window_manager)
    : window_(std::make_unique<shell::CastContentWindowAura>(window_params)),
      extension_host_(std::move(extension_host)),
      window_manager_(window_manager),
      cast_web_contents_(extension_host_->host_contents(), init_params) {
  content::WebContentsObserver::Observe(web_contents());
}

ExtensionPage::~ExtensionPage() {
  content::WebContentsObserver::Observe(nullptr);
}

content::WebContents* ExtensionPage::web_contents() const {
  return extension_host_->host_contents();
}

CastWebContents* ExtensionPage::cast_web_contents() {
  return &cast_web_contents_;
}

void ExtensionPage::Launch() {
  extension_host_->CreateRenderViewSoon();
}

void ExtensionPage::InitializeWindow() {
  window_->GrantScreenAccess();
  window_->CreateWindowForWebContents(
      web_contents(), window_manager_, CastWindowManager::APP,
      chromecast::shell::VisibilityPriority::STICKY_ACTIVITY);
}

void ExtensionPage::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  content::RenderWidgetHostView* view =
      render_view_host->GetWidget()->GetView();
  if (view) {
    view->SetBackgroundColor(SK_ColorTRANSPARENT);
  }
  web_contents()->Focus();
}

}  // namespace chromecast
