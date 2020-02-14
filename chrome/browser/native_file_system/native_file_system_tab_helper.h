// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_TAB_HELPER_H_
#define CHROME_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// This class keeps track of tabs as they're being created, navigated and
// destroyed, informing the permission context when an origin is navigated away
// from. This is then used by the permission context to revoke permissions when
// no top level tabs remain for an origin.
class NativeFileSystemTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NativeFileSystemTabHelper> {
 public:
  ~NativeFileSystemTabHelper() override;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

 private:
  explicit NativeFileSystemTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<NativeFileSystemTabHelper>;

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemTabHelper);
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_TAB_HELPER_H_
