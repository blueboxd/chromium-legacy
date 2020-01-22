// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/overlays/browser_agent/infobar_overlay_browser_agent_util.h"

#include "base/feature_list.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/infobar_overlay_browser_agent.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_interaction_handler.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void AttachInfobarOverlayBrowserAgent(Browser* browser) {
  if (!base::FeatureList::IsEnabled(kInfobarOverlayUI))
    return;
  InfobarOverlayBrowserAgent::CreateForBrowser(browser);
  InfobarOverlayBrowserAgent* browser_agent =
      InfobarOverlayBrowserAgent::FromBrowser(browser);
  browser_agent->AddInfobarInteractionHandler(
      std::make_unique<PasswordInfobarInteractionHandler>(browser));
  // TODO(crbug.com/1030357): Add InfobarInteractionHandlers for each
  // InfobarType when implemented.
}
