// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_DELEGATE_H_

#include "base/memory/scoped_refptr.h"
#include "url/gurl.h"

namespace content_settings {
class CookieSettings;
}

namespace prerender {

// PrerenderManagerDelegate allows content embedders to override
// PrerenderManager logic.
class PrerenderManagerDelegate {
 public:
  virtual ~PrerenderManagerDelegate() = default;

  // Checks whether third party cookies should be blocked.
  virtual scoped_refptr<content_settings::CookieSettings>
  GetCookieSettings() = 0;

  // Perform preconnect, if feasible.
  virtual void MaybePreconnect(const GURL& url) = 0;
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_DELEGATE_H_
