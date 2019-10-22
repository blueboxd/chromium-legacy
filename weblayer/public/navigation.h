// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_NAVIGATION_H_
#define WEBLAYER_PUBLIC_NAVIGATION_H_

#include <vector>

class GURL;

namespace weblayer {

class Navigation {
 public:
  virtual ~Navigation() {}

  // The URL the frame is navigating to. This may change during the navigation
  // when encountering a server redirect.
  virtual GURL GetURL() = 0;

  // Returns the redirects that occurred on the way to the current page. The
  // current page is the last one in the list (so even when there's no redirect,
  // there will be one entry in the list).
  virtual const std::vector<GURL>& GetRedirectChain() = 0;

  // These types are sent over IPC and across different versions. Never remove
  // or change the order.
  enum State {
    kWaitingResponse = 0,
    kReceivingBytes = 1,
    kComplete = 2,
    kFailed = 3,
  };

  virtual State GetState() = 0;

  // TODO(jam): add IsSameDocument
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_NAVIGATION_H_
