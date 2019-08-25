// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_
#define WEBLAYER_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_

#include "weblayer/public/navigation_controller.h"

namespace weblayer {
class BrowserControllerImpl;

class NavigationControllerImpl : public NavigationController {
 public:
  explicit NavigationControllerImpl(BrowserControllerImpl* browser_controller);
  ~NavigationControllerImpl() override;

  // NavigationController implementation:
  void Navigate(const GURL& url) override;

  void GoBack() override;

  void GoForward() override;

  void Reload() override;

  void Stop() override;

 private:
  BrowserControllerImpl* browser_controller_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_
