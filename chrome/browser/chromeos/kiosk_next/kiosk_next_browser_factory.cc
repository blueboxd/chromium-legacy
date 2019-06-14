// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_next/kiosk_next_browser_factory.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

constexpr size_t kMaxKioskNextBrowsersAllowed = 1;

Profile* GetActiveUserProfile() {
  auto* user = user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);
  return profile;
}

Browser* CreateKioskNextBrowser() {
  Browser::CreateParams params(GetActiveUserProfile(), true /* user_gesture */);
  params.type = Browser::Type::TYPE_POPUP; /* Not a tabbed experience. */
  params.trusted_source = true;
  params.initial_show_state = ui::SHOW_STATE_MAXIMIZED;
  params.user_gesture = true;
  Browser* browser = Browser::Create(params);
  DCHECK(browser);
  return browser;
}

void NavigateBrowser(Browser* browser, const GURL& url) {
  NavigateParams params(browser, url, ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  Navigate(&params);
}

}  // namespace

KioskNextBrowserList::Entry::Entry(Browser* browser, const GURL& launch_url)
    : browser(browser), launch_url(launch_url) {}

// static
const KioskNextBrowserList& KioskNextBrowserList::Get() {
  return KioskNextBrowserFactory::Get()->kiosk_next_browser_list();
}

bool KioskNextBrowserList::CanAddBrowser() const {
  return lru_browsers_.size() < max_browsers_allowed_;
}

bool KioskNextBrowserList::IsKioskNextBrowser(const Browser* browser) const {
  return GetBrowserEntry(browser) != nullptr;
}

Browser* KioskNextBrowserList::GetBrowserForWebsite(const GURL& url) const {
  for (auto& entry : lru_browsers_) {
    if (entry->launch_url == url)
      return entry->browser;
  }
  return nullptr;
}

const KioskNextBrowserList::Entry* KioskNextBrowserList::GetBrowserEntry(
    const Browser* browser) const {
  auto iterator = FindBrowser(browser);

  if (iterator != lru_browsers_.end())
    return (*iterator).get();
  return nullptr;
}

void KioskNextBrowserList::OnBrowserRemoved(Browser* browser) {
  auto iterator = FindBrowser(browser);
  if (iterator == lru_browsers_.end())
    return;
  lru_browsers_.erase(iterator);
}

void KioskNextBrowserList::OnBrowserSetLastActive(Browser* browser) {
  auto iterator = FindBrowser(browser);
  if (iterator == lru_browsers_.end())
    return;

  if (browser == lru_browsers_.back()->browser)
    return;

  lru_browsers_.push_back(std::move(*iterator));
  lru_browsers_.erase(iterator);
}

KioskNextBrowserList::KioskNextBrowserList(size_t max_browsers_allowed)
    : max_browsers_allowed_(max_browsers_allowed) {
  BrowserList::GetInstance()->AddObserver(this);
}

KioskNextBrowserList::~KioskNextBrowserList() {
  if (BrowserList::GetInstance())
    BrowserList::GetInstance()->RemoveObserver(this);
}

void KioskNextBrowserList::AddBrowser(Browser* browser, const GURL& url) {
  DCHECK(CanAddBrowser());
  lru_browsers_.push_back(
      std::make_unique<KioskNextBrowserList::Entry>(browser, url));
}

void KioskNextBrowserList::RemoveLRUBrowser() {
  if (lru_browsers_.size() > 0) {
    // TODO(crbug.com/972880) Figure out how to handle the onbeforeunload
    // handlers instead of just skipping them.
    bool result = lru_browsers_.front()->browser->TryToCloseWindow(
        true /* skip_beforeunload */, base::NullCallback());

    DCHECK(!result);

    lru_browsers_.pop_front();
  }
}

KioskNextBrowserList::Iterator KioskNextBrowserList::FindBrowser(
    const Browser* browser) {
  return std::find_if(
      lru_browsers_.begin(), lru_browsers_.end(),
      [browser](const std::unique_ptr<KioskNextBrowserList::Entry>& entry) {
        return browser == entry->browser;
      });
}

KioskNextBrowserList::ConstIterator KioskNextBrowserList::FindBrowser(
    const Browser* browser) const {
  return std::find_if(
      lru_browsers_.begin(), lru_browsers_.end(),
      [browser](const std::unique_ptr<KioskNextBrowserList::Entry>& entry) {
        return browser == entry->browser;
      });
}

// static
KioskNextBrowserFactory* KioskNextBrowserFactory::Get() {
  static base::NoDestructor<KioskNextBrowserFactory> instance(
      kMaxKioskNextBrowsersAllowed);
  return instance.get();
}

KioskNextBrowserFactory::KioskNextBrowserFactory(size_t max_browsers)
    : browser_list_(max_browsers) {}

KioskNextBrowserFactory::~KioskNextBrowserFactory() = default;

Browser* KioskNextBrowserFactory::CreateForWebsite(const GURL& url) {
  Browser* browser = browser_list_.GetBrowserForWebsite(url);
  if (browser) {
    browser_list_.OnBrowserSetLastActive(browser);
    browser->window()->Show();
    return browser;
  }

  if (!browser_list_.CanAddBrowser())
    browser_list_.RemoveLRUBrowser();

  DCHECK(browser_list_.CanAddBrowser());
  browser = CreateKioskNextBrowser();
  browser_list_.AddBrowser(browser, url);

  NavigateBrowser(browser, url);
  browser->window()->Show();
  return browser;
}
