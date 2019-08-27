// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NAVIGATION_IMPL_H_
#define WEBLAYER_BROWSER_NAVIGATION_IMPL_H_

#include "base/macros.h"
#include "weblayer/public/navigation.h"

namespace content {
class NavigationHandle;
}

namespace weblayer {

class NavigationImpl : public Navigation {
 public:
  explicit NavigationImpl(content::NavigationHandle* navigation_handle);
  ~NavigationImpl() override;

 private:
  // Navigation implementation:
  GURL GetURL() override;
  const std::vector<GURL>& GetRedirectChain() override;
  State GetState() override;

  content::NavigationHandle* navigation_handle_;

  DISALLOW_COPY_AND_ASSIGN(NavigationImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NAVIGATION_IMPL_H_
