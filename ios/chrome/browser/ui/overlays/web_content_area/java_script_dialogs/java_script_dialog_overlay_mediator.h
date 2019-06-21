// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_DIALOG_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_DIALOG_OVERLAY_MEDIATOR_H_

#import <Foundation/Foundation.h>

class OverlayRequest;
class JavaScriptDialogSource;
@protocol AlertConsumer;
@protocol JavaScriptDialogOverlayMediatorDelegate;

// Mediator superclass for configuring AlertConsumers for JavaScript dialog
// overlays.
@interface JavaScriptDialogOverlayMediator : NSObject

// The request passed on initialization.
@property(nonatomic, readonly) OverlayRequest* request;

// Returns the source for the OverlayRequest.
@property(nonatomic, readonly) const JavaScriptDialogSource* requestSource;

// The consumer to be updated by this mediator.  Setting to a new value uses the
// configuration data in |request| to update the new consumer.
@property(nonatomic, weak) id<AlertConsumer> consumer;

// The mediator's delegate.
@property(nonatomic, weak) id<JavaScriptDialogOverlayMediatorDelegate> delegate;

// Designated initializer for a mediator that uses |request|'s configuration to
// set up an AlertConsumer.
- (instancetype)initWithRequest:(OverlayRequest*)request
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

// Protocol used by the actions set up by the JavaScriptDialogOverlayMediator.
@protocol JavaScriptDialogOverlayMediatorDelegate <NSObject>

// Called by |mediator| to dismiss the dialog overlay when an action is tapped.
- (void)stopDialogForMediator:(JavaScriptDialogOverlayMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_DIALOG_OVERLAY_MEDIATOR_H_
