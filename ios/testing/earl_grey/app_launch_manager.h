// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_APP_LAUNCH_MANAGER_H_
#define IOS_TESTING_EARL_GREY_APP_LAUNCH_MANAGER_H_

#import <Foundation/Foundation.h>

#include <vector>

#include "base/feature_list.h"

@class AppLaunchManager;

// Enum of relaunch manners. Useful combinations of whether force a relaunch,
// whether kill app gracefully, whether run resets after a relaunch.
typedef NS_ENUM(NSInteger, RelaunchPolicy) {
  // Does not relaunch if app is already running with the same feature list.
  // Kills the app directly. Keeps app state the same as before relaunch.
  NoForceRelaunchAndKeepState,
  // Does not relaunch if app is already running with the same feature list.
  // Kills the app directly. Provides clean test case setups after relaunch.
  NoForceRelaunchAndResetState,
  // Forces a relaunch. Kills the app directly. Keeps app state the same as
  // before relaunch.
  ForceRelaunchByKilling,
  // Forces a relaunch. Backgrounds and then kills the app. Keeps app state same
  // as before relaunch.
  ForceRelaunchByCleanShutdown,
};

// Protocol that test cases can implement to be notified by AppLaunchManager.
@protocol AppLaunchManagerObserver
@optional
// Called when app gets relaunched (due to force restart, or changing the
// arguments).
// |runResets| indicates whether to reset all app status and provide clean test
// case setups.
- (void)appLaunchManagerDidRelaunchApp:(AppLaunchManager*)appLaunchManager
                             runResets:(BOOL)runResets;
@end

// Provides control of the single application-under-test to EarlGrey 2 tests.
@interface AppLaunchManager : NSObject

// Returns the singleton instance of this class.
+ (AppLaunchManager*)sharedManager;

- (instancetype)init NS_UNAVAILABLE;

// Makes sure the app has been started with the appropriate features
// enabled and disabled. Also provides different ways of killing and resetting
// app during relaunch.
// In EG2, the app will be launched from scratch if:
// * The app is not running, or
// * The app is currently running with a different feature set, or
// * |relaunchPolicy| forces a relaunch.
// Otherwise, the app will be activated instead of (re)launched.
// Will wait until app is activated or launched, and fail the test if it
// fails to do so.
// |relaunchPolicy| controls relaunch manners.
- (void)ensureAppLaunchedWithFeaturesEnabled:
            (const std::vector<base::Feature>&)featuresEnabled
                                    disabled:(const std::vector<base::Feature>&)
                                                 featuresDisabled
                              relaunchPolicy:(RelaunchPolicy)relaunchPolicy;

// Moves app to background and then moves it back. In EG1, this method is a
// no-op.
- (void)backgroundAndForegroundApp;

// Adds an observer for AppLaunchManager.
- (void)addObserver:(id<AppLaunchManagerObserver>)observer;

// Removes an observer for AppLaunchManager.
- (void)removeObserver:(id<AppLaunchManagerObserver>)observer;

@end

#endif  // IOS_TESTING_EARL_GREY_APP_LAUNCH_MANAGER_H_
