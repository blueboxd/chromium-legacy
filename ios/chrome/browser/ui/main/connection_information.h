// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_CONNECTION_INFORMATION_H_
#define IOS_CHROME_BROWSER_UI_MAIN_CONNECTION_INFORMATION_H_

#import <Foundation/Foundation.h>

@class AppStartupParameters;

// Contains information about the initialization of scenes.
@protocol ConnectionInformation <NSObject>

// Parameters received when the scene is connected. These parameters are stored
// to be executed when the scene reach the required state.
@property(nonatomic, strong) AppStartupParameters* startupParameters;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_CONNECTION_INFORMATION_H_
