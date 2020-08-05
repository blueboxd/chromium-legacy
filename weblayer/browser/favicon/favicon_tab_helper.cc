// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/favicon/favicon_tab_helper.h"

#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "weblayer/browser/favicon/favicon_service_impl.h"
#include "weblayer/browser/favicon/favicon_service_impl_factory.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/public/favicon_fetcher_delegate.h"

namespace weblayer {

FaviconTabHelper::ObserverSubscription::ObserverSubscription(
    FaviconTabHelper* helper,
    FaviconFetcherDelegate* delegate)
    : helper_(helper), delegate_(delegate) {
  helper_->AddDelegate(delegate_);
}

FaviconTabHelper::ObserverSubscription::~ObserverSubscription() {
  helper_->RemoveDelegate(delegate_);
}

FaviconTabHelper::~FaviconTabHelper() {
  // All of the ObserverSubscriptions should have been destroyed before this.
  DCHECK_EQ(0, observer_count_);
}

std::unique_ptr<FaviconTabHelper::ObserverSubscription>
FaviconTabHelper::RegisterFaviconFetcherDelegate(
    FaviconFetcherDelegate* delegate) {
  // WrapUnique as constructor is private.
  return base::WrapUnique(new ObserverSubscription(this, delegate));
}

FaviconTabHelper::FaviconTabHelper(content::WebContents* contents)
    : WebContentsObserver(contents) {
  // This code relies on the ability to get a Profile for the BrowserContext.
  DCHECK(ProfileImpl::FromBrowserContext(web_contents()->GetBrowserContext()));
}

void FaviconTabHelper::AddDelegate(FaviconFetcherDelegate* delegate) {
  delegates_.AddObserver(delegate);
  if (++observer_count_ == 1) {
    ProfileImpl* profile =
        ProfileImpl::FromBrowserContext(web_contents()->GetBrowserContext());
    FaviconServiceImpl* favicon_service =
        FaviconServiceImplFactory::GetForProfile(profile);
    favicon::ContentFaviconDriver::CreateForWebContents(web_contents(),
                                                        favicon_service);
    favicon::ContentFaviconDriver::FromWebContents(web_contents())
        ->AddObserver(this);
  }
}

void FaviconTabHelper::RemoveDelegate(FaviconFetcherDelegate* delegate) {
  delegates_.RemoveObserver(delegate);
  --observer_count_;
  DCHECK_GE(observer_count_, 0);
  if (observer_count_ == 0) {
    favicon::ContentFaviconDriver::FromWebContents(web_contents())
        ->RemoveObserver(this);
    // ContentFaviconDriver downloads images, if there are no observers there
    // is no need to keep it around. This triggers deleting it.
    web_contents()->SetUserData(favicon::ContentFaviconDriver::UserDataKey(),
                                nullptr);
    favicon_ = gfx::Image();
  }
}

void FaviconTabHelper::OnFaviconUpdated(
    favicon::FaviconDriver* favicon_driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  favicon_ = image;
  for (FaviconFetcherDelegate& delegate : delegates_)
    delegate.OnFaviconChanged(favicon_);
}

void FaviconTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  favicon_ = gfx::Image();
  // Don't send notification in this case as it's assumed a new navigation
  // triggers resetting the favicon.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FaviconTabHelper)

}  // namespace weblayer
