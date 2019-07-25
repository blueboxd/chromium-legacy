// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_HTTP_AUTH_DIALOGS_HTTP_AUTH_DIALOG_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_HTTP_AUTH_DIALOGS_HTTP_AUTH_DIALOG_OVERLAY_MEDIATOR_H_

#import <Foundation/Foundation.h>

class OverlayRequest;
class HTTPAuthOverlayRequestConfig;
@protocol HTTPAuthDialogOverlayMediatorDelegate;
@protocol HTTPAuthDialogOverlayMediatorDataSource;
@protocol AlertConsumer;

// Mediator object that uses a HTTPAuthOverlayRequestConfig to set up the UI for
// an HTTP authentication dialog.
@interface HTTPAuthDialogOverlayMediator : NSObject

// The consumer to be updated by this mediator.  Setting to a new value uses the
// HTTPAuthOverlayRequestConfig to update the new consumer.
@property(nonatomic, weak) id<AlertConsumer> consumer;

// The delegate that handles action button functionality set up by the mediator.
@property(nonatomic, weak) id<HTTPAuthDialogOverlayMediatorDelegate> delegate;

// The datasource for prompt input values.
@property(nonatomic, weak) id<HTTPAuthDialogOverlayMediatorDataSource>
    dataSource;

// Designated initializer for a mediator that uses |request|'s configuration to
// set up an AlertConsumer.
- (instancetype)initWithRequest:(OverlayRequest*)request;

@end

// Protocol used by the actions set up by the
// HTTPAuthDialogOverlayMediator.
@protocol HTTPAuthDialogOverlayMediatorDelegate <NSObject>

// Called by |mediator| to dismiss the dialog overlay when
// an action is tapped.
- (void)stopDialogForMediator:(HTTPAuthDialogOverlayMediator*)mediator;

@end

// Protocol used to provide the text input from the HTTP authentication UI to
// the mediator.
@protocol HTTPAuthDialogOverlayMediatorDataSource <NSObject>

// Fetches the current value of the username and password text fields from the
// UI that was set up by |mediator|.
- (NSString*)userForMediator:(HTTPAuthDialogOverlayMediator*)mediator;
- (NSString*)passwordForMediator:(HTTPAuthDialogOverlayMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_HTTP_AUTH_DIALOGS_HTTP_AUTH_DIALOG_OVERLAY_MEDIATOR_H_
