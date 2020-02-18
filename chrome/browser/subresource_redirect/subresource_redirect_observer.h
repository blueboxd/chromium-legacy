// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_OBSERVER_H_
#define CHROME_BROWSER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_OBSERVER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace subresource_redirect {

// Sends the public image URL hints to renderer.
class SubresourceRedirectObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SubresourceRedirectObserver> {
 public:
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  ~SubresourceRedirectObserver() override;
  SubresourceRedirectObserver(const SubresourceRedirectObserver&) = delete;
  SubresourceRedirectObserver& operator=(const SubresourceRedirectObserver&) =
      delete;

 private:
  friend class content::WebContentsUserData<SubresourceRedirectObserver>;

  explicit SubresourceRedirectObserver(content::WebContents* web_contents);

  // content::WebContentsObserver.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace subresource_redirect

#endif  // CHROME_BROWSER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_OBSERVER_H_
