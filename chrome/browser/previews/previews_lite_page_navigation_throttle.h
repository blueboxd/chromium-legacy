// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_H_

#include "components/previews/content/previews_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

class PreviewsLitePageDecider;

// If the given URL is a LitePage Preview URL, this returns true but does not
// change the |url|. This will set |update_virtual_url_with_url| on
// NavigationEntry so that |HandlePreviewsLitePageURLRewriteReverse| is called
// when the navigation finishes.
// Note: This means the virtual URL will not be set during the navigation load.
// This is handled separately in UI on Android.
bool HandlePreviewsLitePageURLRewrite(GURL* url,
                                      content::BrowserContext* browser_context);

// Handles translating the given Lite Page URL to the original URL. Returns true
// if the given |url| was a preview, otherwise returns false and does not change
// |url|.
bool HandlePreviewsLitePageURLRewriteReverse(
    GURL* url,
    content::BrowserContext* browser_context);

class PreviewsLitePageNavigationThrottle {
 public:
  // Returns the URL for a preview given by the url.
  static GURL GetPreviewsURLForURL(const GURL& original_url);

  // Gets the ServerLitePageInfo struct from an existing attempted lite page
  // navigation, if there is one. If not, returns a new ServerLitePageInfo
  // initialized with metadata from navigation_handle() and |this| that is owned
  // by the PreviewsUserData associated with navigation_handle().
  static previews::PreviewsUserData::ServerLitePageInfo*
  GetOrCreateServerLitePageInfo(content::NavigationHandle* navigation_handle,
                                PreviewsLitePageDecider* manager);

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageNavigationThrottle);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_H_
