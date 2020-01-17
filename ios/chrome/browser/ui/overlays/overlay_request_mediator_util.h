// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_REQUEST_MEDIATOR_UTIL_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_REQUEST_MEDIATOR_UTIL_H_

#import <Foundation/Foundation.h>

#include <memory>

class OverlayRequestSupport;

// Returns an OverlayRequestSupport aggregating the support from all
// OverlayRequestMediators in |mediator_classes|.  Used to create the support
// for OverlayRequestCoordinators that manage more than one
// OverlayRequestMediator type.
std::unique_ptr<OverlayRequestSupport> CreateAggregateSupportForMediators(
    NSArray<Class>* mediator_classes);

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_REQUEST_MEDIATOR_UTIL_H_
