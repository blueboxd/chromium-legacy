// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_H_

#import <Foundation/Foundation.h>

enum class InfobarType;

// Protocol for the InfobarCoordinators to communicate with the InfobarContainer
// Coordinator.
@protocol InfobarContainer

// Informs the InfobarContainer Coordinator that its child coordinator of type
// |infobarType| has dismissed its banner.
- (void)childCoordinatorBannerWasDismissed:(InfobarType)infobarType;

// Informs the InfobarContainer Coordinator that its child coordinator of type
// |infobarType| has stopped.
- (void)childCoordinatorStopped:(InfobarType)infobarType;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_H_
