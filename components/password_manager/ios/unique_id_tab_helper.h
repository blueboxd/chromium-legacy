// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_UNIQUE_ID_TAB_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_UNIQUE_ID_TAB_HELPER_H_

#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// Class binding a PasswordController to a WebState.
class UniqueIDTabHelper : public web::WebStateObserver,
                          public web::WebStateUserData<UniqueIDTabHelper> {
 public:
  ~UniqueIDTabHelper() override;

  // Returns the next available renderer id for WebState.
  uint32_t GetNextAvailableRendererID();

  void SetNextAvailableRendererID(uint32_t new_id);

 private:
  friend class web::WebStateUserData<UniqueIDTabHelper>;

  explicit UniqueIDTabHelper(web::WebState* web_state);

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;

  uint32_t next_available_renderer_id_ = 0;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(UniqueIDTabHelper);
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_TAB_HELPER_H_
