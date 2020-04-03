// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_TESTING_H_

#import "ios/chrome/browser/procedural_block_types.h"

@protocol TabSwitcher;

// Methods exposed for testing. This is terrible and should be rewritten.
@interface SceneController ()

- (void)showFirstRunUI;
- (void)setTabSwitcher:(id<TabSwitcher>)switcher;
- (id<TabSwitcher>)tabSwitcher;
- (BOOL)isTabSwitcherActive;

// Dismisses all modal dialogs, excluding the omnibox if |dismissOmnibox| is
// NO, then call |completion|.
- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_TESTING_H_
