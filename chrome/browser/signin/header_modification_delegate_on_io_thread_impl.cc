// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/header_modification_delegate_on_io_thread_impl.h"

#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_navigation_ui_data.h"

namespace signin {

HeaderModificationDelegateOnIOThreadImpl::
    HeaderModificationDelegateOnIOThreadImpl(
        content::ResourceContext* resource_context)
    : io_data_(ProfileIOData::FromResourceContext(resource_context)) {}

HeaderModificationDelegateOnIOThreadImpl::
    ~HeaderModificationDelegateOnIOThreadImpl() = default;

bool HeaderModificationDelegateOnIOThreadImpl::ShouldInterceptNavigation(
    content::NavigationUIData* navigation_ui_data) {
  if (io_data_->IsOffTheRecord())
    return false;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Note: InlineLoginUI uses an isolated request context and thus should
  // bypass the account consistency flow. See http://crbug.com/428396
  ChromeNavigationUIData* chrome_navigation_ui_data =
      static_cast<ChromeNavigationUIData*>(navigation_ui_data);
  if (chrome_navigation_ui_data) {
    extensions::ExtensionNavigationUIData* extension_navigation_ui_data =
        chrome_navigation_ui_data->GetExtensionNavigationUIData();
    if (extension_navigation_ui_data &&
        extension_navigation_ui_data->is_web_view()) {
      return false;
    }
  }
#endif

  return true;
}

void HeaderModificationDelegateOnIOThreadImpl::ProcessRequest(
    ChromeRequestAdapter* request_adapter,
    const GURL& redirect_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  FixAccountConsistencyRequestHeader(
      request_adapter, redirect_url, io_data_->IsOffTheRecord(),
      io_data_->incognito_availibility()->GetValue(),
      io_data_->account_consistency(),
      io_data_->google_services_account_id()->GetValue(),
#if defined(OS_CHROMEOS)
      io_data_->account_consistency_mirror_required()->GetValue(),
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      io_data_->IsSyncEnabled(), io_data_->GetSigninScopedDeviceId(),
#endif
      io_data_->GetCookieSettings());
}

void HeaderModificationDelegateOnIOThreadImpl::ProcessResponse(
    ResponseAdapter* response_adapter,
    const GURL& redirect_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  ProcessAccountConsistencyResponseHeaders(response_adapter, redirect_url,
                                           io_data_->IsOffTheRecord());
}

}  // namespace signin
