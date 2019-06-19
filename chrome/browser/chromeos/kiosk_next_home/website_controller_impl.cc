// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_next_home/website_controller_impl.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/chromeos/kiosk_next/kiosk_next_browser_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace chromeos {
namespace kiosk_next_home {
namespace {

Profile* GetActiveUserProfile() {
  auto* user = user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);
  return profile;
}

}  // namespace

WebsiteControllerImpl::WebsiteControllerImpl(
    mojom::WebsiteControllerRequest request)
    : binding_(this, std::move(request)) {}

WebsiteControllerImpl::~WebsiteControllerImpl() = default;

void WebsiteControllerImpl::LaunchKioskNextWebsite(const GURL& url) {
  KioskNextBrowserFactory::Get()->CreateForWebsite(url);
}

void WebsiteControllerImpl::LaunchWebsite(const GURL& url) {
  Browser::CreateParams create_params(GetActiveUserProfile(),
                                      true /* user_gesture */);
  Browser* browser = Browser::Create(create_params);

  NavigateParams navigate_params(browser, url,
                                 ui::PageTransition::PAGE_TRANSITION_LINK);
  navigate_params.disposition = WindowOpenDisposition::CURRENT_TAB;
  Navigate(&navigate_params);

  browser->window()->Show();
}

}  // namespace kiosk_next_home
}  // namespace chromeos
