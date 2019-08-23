// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/web_browser_controller_impl.h"

#include "base/logging.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"
#include "weblayer/browser/web_navigation_controller_impl.h"
#include "weblayer/browser/web_profile_impl.h"

namespace weblayer {

WebBrowserControllerImpl::WebBrowserControllerImpl(
    WebProfile* profile,
    const gfx::Size& initial_size)
    : profile_(profile) {
  auto* profile_impl = static_cast<WebProfileImpl*>(profile_);
  content::WebContents::CreateParams create_params(
      profile_impl->GetBrowserContext());
  create_params.initial_size = initial_size;
  web_contents_ = content::WebContents::Create(create_params);

  web_navigation_controller_ =
      std::make_unique<WebNavigationControllerImpl>(this);
}

WebBrowserControllerImpl::~WebBrowserControllerImpl() = default;

WebNavigationController*
WebBrowserControllerImpl::GetWebNavigationController() {
  return web_navigation_controller_.get();
}

void WebBrowserControllerImpl::AttachToView(views::WebView* web_view) {
  web_view->SetWebContents(web_contents_.get());
  web_contents_->Focus();
}

std::unique_ptr<WebBrowserController> WebBrowserController::Create(
    WebProfile* profile,
    const gfx::Size& initial_size) {
  return std::make_unique<WebBrowserControllerImpl>(profile, initial_size);
}

}  // namespace weblayer
