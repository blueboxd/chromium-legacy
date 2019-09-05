// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/appearance/appearance_customization.h"

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/common/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void CustomizeUIAppearance() {
  UIView.appearance.tintColor = [UIColor colorNamed:kBlueColor];
  UISwitch.appearance.onTintColor = [UIColor colorNamed:kBlueColor];
}
