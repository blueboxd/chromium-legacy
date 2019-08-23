// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_WEB_NAVIGATION_CONTROLLER_H_
#define WEBLAYER_PUBLIC_WEB_NAVIGATION_CONTROLLER_H_

#include <algorithm>

class GURL;

namespace weblayer {

class WebNavigationController {
 public:
  virtual ~WebNavigationController() {}

  virtual void Navigate(const GURL& url) = 0;

  virtual void GoBack() = 0;

  virtual void GoForward() = 0;

  virtual void Reload() = 0;

  virtual void Stop() = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_WEB_NAVIGATION_CONTROLLER_H_
