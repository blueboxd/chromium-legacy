// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/navigation_controller_impl.h"

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "weblayer/browser/browser_controller_impl.h"

namespace weblayer {

NavigationControllerImpl::NavigationControllerImpl(
    BrowserControllerImpl* browser_controller)
    : browser_controller_(browser_controller) {}

NavigationControllerImpl::~NavigationControllerImpl() = default;

void NavigationControllerImpl::Navigate(const GURL& url) {
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  browser_controller_->web_contents()->GetController().LoadURLWithParams(
      params);
  // So that if the user had entered the UI in a bar it stops flashing the
  // caret.
  browser_controller_->web_contents()->Focus();
}

void NavigationControllerImpl::GoBack() {
  browser_controller_->web_contents()->GetController().GoBack();
}

void NavigationControllerImpl::GoForward() {
  browser_controller_->web_contents()->GetController().GoForward();
}

void NavigationControllerImpl::Reload() {
  browser_controller_->web_contents()->GetController().Reload(
      content::ReloadType::NORMAL, false);
}

void NavigationControllerImpl::Stop() {
  browser_controller_->web_contents()->Stop();
}

}  // namespace weblayer
