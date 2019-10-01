// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kInfobarUIReboot{"InfobarUIReboot",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Feature enabled by default since it will always be checked along
// kInfobarUIReboot, effectively working as a kill switch. Meaning that if
// kInfobarUIReboot is not enabled this feature won't work.
const base::Feature kConfirmInfobarMessagesUI{"ConfirmInfobarMessagesUI",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

bool IsInfobarUIRebootEnabled() {
  return base::FeatureList::IsEnabled(kInfobarUIReboot);
}

bool IsConfirmInfobarMessagesUIEnabled() {
  return base::FeatureList::IsEnabled(kConfirmInfobarMessagesUI) &&
         IsInfobarUIRebootEnabled();
}
