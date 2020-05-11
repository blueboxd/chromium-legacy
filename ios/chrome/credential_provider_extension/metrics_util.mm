// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/metrics_util.h"

#include "ios/chrome/common/app_group/app_group_metrics.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void UpdateUMACountForKey(NSString* key) {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSInteger numberOfDisplay = [sharedDefaults integerForKey:key];
  [sharedDefaults setInteger:numberOfDisplay + 1 forKey:key];
}
