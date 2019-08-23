// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEB_PROFILE_IMPL_H_
#define WEBLAYER_BROWSER_WEB_PROFILE_IMPL_H_

#include "weblayer/public/web_profile.h"

namespace content {
class BrowserContext;
}

namespace weblayer {

class WebProfileImpl : public WebProfile {
 public:
  explicit WebProfileImpl(const base::FilePath& path);
  ~WebProfileImpl() override;

  content::BrowserContext* GetBrowserContext();

  // WebProfile implementation:
  void ClearBrowsingData() override;

 private:
  class WebBrowserContext;

  base::FilePath path_;
  std::unique_ptr<WebBrowserContext> browser_context_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEB_PROFILE_IMPL_H_
