// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_TAB_HELPER_H_
#define CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class KaleidoscopeTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<KaleidoscopeTabHelper> {
 public:
  ~KaleidoscopeTabHelper() override;
  KaleidoscopeTabHelper(const KaleidoscopeTabHelper&) = delete;
  KaleidoscopeTabHelper& operator=(const KaleidoscopeTabHelper&) = delete;

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(content::NavigationHandle* handle) override;

 private:
  friend class content::WebContentsUserData<KaleidoscopeTabHelper>;

  explicit KaleidoscopeTabHelper(content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_TAB_HELPER_H_
