// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_WEB_CONTENTS_OBSERVER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}  // namespace content

class OptimizationGuideKeyedService;

// Observes navigation events.
class OptimizationGuideWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          OptimizationGuideWebContentsObserver> {
 public:
  ~OptimizationGuideWebContentsObserver() override;

 private:
  friend class content::WebContentsUserData<
      OptimizationGuideWebContentsObserver>;

  explicit OptimizationGuideWebContentsObserver(
      content::WebContents* web_contents);

  // Overridden from content::WebContentsObserver.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Initialized in constructor. It may be null if the
  // OptimizationGuideKeyedService feature is not enabled.
  OptimizationGuideKeyedService* optimization_guide_keyed_service_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideWebContentsObserver);
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_WEB_CONTENTS_OBSERVER_H_
