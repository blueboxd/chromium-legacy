// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/overlays/infobar_overlay_browser_agent_util.h"

#include "base/feature_list.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"

void AttachInfobarOverlayBrowserAgents(Browser* browser) {
  if (!base::FeatureList::IsEnabled(kInfobarOverlayUI))
    return;
  // TODO(crbug.com/1030357): Create browser agents.
}
