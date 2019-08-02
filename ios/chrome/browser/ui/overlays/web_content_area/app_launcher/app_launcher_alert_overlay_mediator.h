// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_APP_LAUNCHER_APP_LAUNCHER_ALERT_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_APP_LAUNCHER_APP_LAUNCHER_ALERT_OVERLAY_MEDIATOR_H_

#import <Foundation/Foundation.h>

class OverlayRequest;
@protocol AppLauncherAlertOverlayMediatorDelegate;
@protocol AppLauncherAlertOverlayMediatorDataSource;
@protocol AlertConsumer;

// Mediator object that uses a AppLauncherAlertOverlayRequestConfig to set up
// the UI for an alert notifying the user that a navigation will open an
// external app.
@interface AppLauncherAlertOverlayMediator : NSObject

// The consumer to be updated by this mediator.  Setting to a new value uses the
// AppLauncherAlertOverlayRequestConfig to update the new consumer.
@property(nonatomic, weak) id<AlertConsumer> consumer;

// The delegate that handles action button functionality set up by the mediator.
@property(nonatomic, weak) id<AppLauncherAlertOverlayMediatorDelegate> delegate;

// Designated initializer for a mediator that uses |request|'s configuration to
// set up an AlertConsumer.
- (instancetype)initWithRequest:(OverlayRequest*)request;

@end

// Protocol used by the actions set up by the
// AppLauncherAlertOverlayMediator.
@protocol AppLauncherAlertOverlayMediatorDelegate <NSObject>

// Called by |mediator| to dismiss the dialog overlay when
// an action is tapped.
- (void)stopDialogForMediator:(AppLauncherAlertOverlayMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_APP_LAUNCHER_APP_LAUNCHER_ALERT_OVERLAY_MEDIATOR_H_
