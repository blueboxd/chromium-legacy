// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOGIN_LOGIN_TAB_HELPER_H_
#define CHROME_BROWSER_UI_LOGIN_LOGIN_TAB_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class LoginDelegate;
class NavigationHandle;
class WebContents;
}  // namespace content

// LoginTabHelper is responsible for observing navigations that need to trigger
// authentication prompts, showing the login prompt, and handling user-entered
// credentials from the prompt.
class LoginTabHelper : public content::WebContentsObserver,
                       public content::WebContentsUserData<LoginTabHelper> {
 public:
  ~LoginTabHelper() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Returns false if the omnibox should hide the URL due to a proxy auth
  // prompt, and true otherwise. The URL should not be shown during a proxy auth
  // prompt to avoid origin confusion.
  bool ShouldDisplayURL() const;
  // Returns true if an auth prompt is currently visible.
  bool IsShowingPrompt() const;

 private:
  friend class content::WebContentsUserData<LoginTabHelper>;

  explicit LoginTabHelper(content::WebContents* web_contents);

  void HandleCredentials(
      const base::Optional<net::AuthCredentials>& credentials);

  // When the user enters credentials into the login prompt, they are populated
  // in the auth cache and then page is reloaded to re-send the request with the
  // cached credentials. This method is passed as the callback to the call that
  // places the credentials into the cache.
  void Reload();

  std::unique_ptr<content::LoginDelegate> delegate_;
  GURL url_for_delegate_;

  net::AuthChallengeInfo challenge_;

  base::WeakPtrFactory<LoginTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(LoginTabHelper);
};

#endif  // CHROME_BROWSER_UI_LOGIN_LOGIN_TAB_HELPER_H_
