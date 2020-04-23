// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_SCENE_STATE_H_
#define IOS_CHROME_BROWSER_UI_MAIN_SCENE_STATE_H_

#import <UIKit/UIKit.h>

@class SceneController;
@class SceneState;
@protocol BrowserInterfaceProvider;

// Describes the possible scene states.
// This is an iOS 12 compatible version of UISceneActivationState enum.
typedef NS_ENUM(NSUInteger, SceneActivationLevel) {
  // The scene is not connected and has no window.
  SceneActivationLevelUnattached = 0,
  // The scene is connected, and has a window associated with it. The window is
  // not visible to the user, except possibly in the app switcher.
  SceneActivationLevelBackground,
  // The scene is connected, and its window is on screen, but it's not active
  // for user input. For example, keyboard events would not be sent to this
  // window.
  SceneActivationLevelForegroundInactive,
  // The scene is connected, has a window, and receives user events.
  SceneActivationLevelForegroundActive
};

@protocol SceneStateObserver <NSObject>

@optional

// Called whenever the scene state transitions between different activity
// states.
- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level;

@end

// An object containing the state of a UIWindowScene. One state object
// corresponds to one scene.
@interface SceneState : NSObject

// The current activation level.
@property(nonatomic, assign) SceneActivationLevel activationLevel;

// Window ID, used for restoration.
// TODO(crbug.com/1069762): remove this.
@property(nonatomic, assign, readonly) NSUInteger windowID;

// Window for the associated scene, if any.
@property(nonatomic, strong) UIWindow* window;

@property(nonatomic, strong) UIWindowScene* scene API_AVAILABLE(ios(13));

@property(nonatomic, strong)
    UISceneConnectionOptions* connectionOptions API_AVAILABLE(ios(13));

// The interface provider associated with this scene.
@property(nonatomic, strong, readonly) id<BrowserInterfaceProvider>
    interfaceProvider;

// True if First Run UI (terms of service & sync sign-in) is being presented
// in a modal dialog.
@property(nonatomic, assign) BOOL presentingFirstRunUI;

// The controller for this scene.
@property(nonatomic, weak) SceneController* controller;

// Adds an observer to this scene state. The observers will be notified about
// scene state changes per SceneStateObserver protocol.
- (void)addObserver:(id<SceneStateObserver>)observer;
// Removes the observer. It's safe to call this at any time, including from
// SceneStateObserver callbacks.
- (void)removeObserver:(id<SceneStateObserver>)observer;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_SCENE_STATE_H_
