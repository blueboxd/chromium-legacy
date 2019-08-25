// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_controller_impl.h"

#include "base/logging.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"
#include "weblayer/browser/navigation_controller_impl.h"
#include "weblayer/browser/profile_impl.h"

namespace weblayer {

BrowserControllerImpl::BrowserControllerImpl(Profile* profile,
                                             const gfx::Size& initial_size)
    : profile_(profile) {
  auto* profile_impl = static_cast<ProfileImpl*>(profile_);
  content::WebContents::CreateParams create_params(
      profile_impl->GetBrowserContext());
  create_params.initial_size = initial_size;
  web_contents_ = content::WebContents::Create(create_params);

  navigation_controller_ = std::make_unique<NavigationControllerImpl>(this);
}

BrowserControllerImpl::~BrowserControllerImpl() = default;

NavigationController* BrowserControllerImpl::GetNavigationController() {
  return navigation_controller_.get();
}

void BrowserControllerImpl::AttachToView(views::WebView* web_view) {
  web_view->SetWebContents(web_contents_.get());
  web_contents_->Focus();
}

std::unique_ptr<BrowserController> BrowserController::Create(
    Profile* profile,
    const gfx::Size& initial_size) {
  return std::make_unique<BrowserControllerImpl>(profile, initial_size);
}

}  // namespace weblayer
