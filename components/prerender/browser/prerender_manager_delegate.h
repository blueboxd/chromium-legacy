// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRERENDER_BROWSER_PRERENDER_MANAGER_DELEGATE_H_
#define COMPONENTS_PRERENDER_BROWSER_PRERENDER_MANAGER_DELEGATE_H_

#include "base/memory/scoped_refptr.h"
#include "components/prerender/browser/prerender_contents_delegate.h"
#include "components/prerender/common/prerender_origin.h"
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

  // Get the prerender contents delegate.
  virtual std::unique_ptr<PrerenderContentsDelegate>
  GetPrerenderContentsDelegate() = 0;

  // Check whether predictive loading of web pages is enabled for |origin|.
  virtual bool IsPredictionEnabled(Origin origin) = 0;

  // Check whether predictive loading of web pages is enabled.
  virtual bool IsPredictionEnabled() = 0;

  // Check whether predictive loading of web pages is disabled due to network.
  virtual bool IsPredictionDisabledDueToNetwork(Origin origin) = 0;

  // Gets the reason why predictive loading of web pages was disabld.
  virtual std::string GetReasonForDisablingPrediction() = 0;
};

}  // namespace prerender

#endif  // COMPONENTS_PRERENDER_BROWSER_PRERENDER_MANAGER_DELEGATE_H_
