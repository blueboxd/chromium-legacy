// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/navigation_error_navigation_throttle.h"

#include "content/public/browser/navigation_handle.h"
#include "net/base/net_errors.h"
#include "weblayer/browser/navigation_controller_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/error_page.h"
#include "weblayer/public/error_page_delegate.h"

using content::NavigationThrottle;

namespace weblayer {

NavigationErrorNavigationThrottle::NavigationErrorNavigationThrottle(
    content::NavigationHandle* handle)
    : NavigationThrottle(handle) {
  // As this calls to the delegate, and the delegate only knows about main
  // frames, this should only be used for main frames.
  DCHECK(handle->IsInMainFrame());
}

NavigationErrorNavigationThrottle::~NavigationErrorNavigationThrottle() =
    default;

NavigationThrottle::ThrottleCheckResult
NavigationErrorNavigationThrottle::WillFailRequest() {
  // The embedder is not allowed to replace ssl error pages.
  if (navigation_handle()->GetNetErrorCode() == net::Error::OK ||
      net::IsCertificateError(navigation_handle()->GetNetErrorCode())) {
    return NavigationThrottle::PROCEED;
  }

  TabImpl* tab =
      TabImpl::FromWebContents(navigation_handle()->GetWebContents());
  // Instances of this class are only created if there is a Tab associated
  // with the WebContents.
  DCHECK(tab);
  if (!tab->error_page_delegate())
    return NavigationThrottle::PROCEED;

  NavigationImpl* navigation =
      static_cast<NavigationControllerImpl*>(tab->GetNavigationController())
          ->GetNavigationImplFromHandle(navigation_handle());
  // The navigation this was created for should always outlive this.
  DCHECK(navigation);
  auto error_page = tab->error_page_delegate()->GetErrorPageContent(navigation);
  if (error_page) {
    return NavigationThrottle::ThrottleCheckResult(
        NavigationThrottle::BLOCK_REQUEST,
        navigation_handle()->GetNetErrorCode(), error_page->html);
  }
  return NavigationThrottle::PROCEED;
}

const char* NavigationErrorNavigationThrottle::GetNameForLogging() {
  return "NavigationErrorNavigationThrottle";
}

}  // namespace weblayer
