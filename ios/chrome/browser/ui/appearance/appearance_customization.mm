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
  Class containerClass = [TableViewNavigationController class];
  UIBarButtonItem* barButtonItemAppearance = [UIBarButtonItem
      appearanceWhenContainedInInstancesOfClasses:@[ containerClass ]];
  barButtonItemAppearance.tintColor = [UIColor colorNamed:kTintColor];

  Class navigationBarClass = [SettingsNavigationController class];
  UINavigationBar* navigationBarAppearance = [UINavigationBar
      appearanceWhenContainedInInstancesOfClasses:@[ navigationBarClass ]];
  navigationBarAppearance.tintColor = [UIColor colorNamed:kTintColor];
}
