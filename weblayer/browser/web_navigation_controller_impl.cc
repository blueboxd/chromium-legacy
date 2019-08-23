// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/web_navigation_controller_impl.h"

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "weblayer/browser/web_browser_controller_impl.h"

namespace weblayer {

WebNavigationControllerImpl::WebNavigationControllerImpl(
    WebBrowserControllerImpl* web_browser_controller)
    : web_browser_controller_(web_browser_controller) {}

WebNavigationControllerImpl::~WebNavigationControllerImpl() = default;

void WebNavigationControllerImpl::Navigate(const GURL& url) {
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_browser_controller_->web_contents()->GetController().LoadURLWithParams(
      params);
  // So that if the user had entered the UI in a bar it stops flashing the
  // caret.
  web_browser_controller_->web_contents()->Focus();
}

void WebNavigationControllerImpl::GoBack() {
  web_browser_controller_->web_contents()->GetController().GoBack();
}

void WebNavigationControllerImpl::GoForward() {
  web_browser_controller_->web_contents()->GetController().GoForward();
}

void WebNavigationControllerImpl::Reload() {
  web_browser_controller_->web_contents()->GetController().Reload(
      content::ReloadType::NORMAL, false);
}

void WebNavigationControllerImpl::Stop() {
  web_browser_controller_->web_contents()->Stop();
}

}  // namespace weblayer
