// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_FAVICON_FAVICON_MANAGER_H_
#define IOS_WEB_FAVICON_FAVICON_MANAGER_H_

#include "base/macros.h"
#include "base/values.h"

class GURL;
namespace web {
class WebStateImpl;
class WebFrame;

// Handles "favicon.favicons" message from injected JavaScript and notifies
// WebStateImpl if message contains favicon URLs.
class FaviconManager final {
 public:
  explicit FaviconManager(WebStateImpl* web_state);
  ~FaviconManager();

 private:
  bool OnJsMessage(const base::DictionaryValue& message,
                   const GURL& page_url,
                   bool has_user_gesture,
                   bool in_main_frame,
                   WebFrame* sender_frame);

  WebStateImpl* web_state_impl_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FaviconManager);
};

}  // namespace web

#endif  // IOS_WEB_FAVICON_FAVICON_MANAGER_H_
