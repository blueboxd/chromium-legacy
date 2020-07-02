// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SCOPED_UI_BLOCKER_H_
#define IOS_CHROME_APP_SCOPED_UI_BLOCKER_H_

#include "base/logging.h"
#include "base/macros.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/ui/main/scene_state.h"

// A helper object that increments AppState's blocking UI counter for
// its entire lifetime.
class ScopedUIBlocker {
 public:
  explicit ScopedUIBlocker(SceneState* scene);
  ~ScopedUIBlocker();

 private:
  // The scene showing the blocking UI.
  __weak SceneState* scene_;

  ScopedUIBlocker(const ScopedUIBlocker&) = delete;
  ScopedUIBlocker& operator=(const ScopedUIBlocker&) = delete;
};

#endif  // IOS_CHROME_APP_SCOPED_UI_BLOCKER_H_
