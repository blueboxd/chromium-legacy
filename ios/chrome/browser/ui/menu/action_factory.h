// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MENU_ACTION_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_MENU_ACTION_FACTORY_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/menu/menu_action_type.h"

class GURL;

// Factory providing methods to create UIActions with consistent titles, images
// and metrics structure.
API_AVAILABLE(ios(13.0))
@interface ActionFactory : NSObject

// Initializes a factory instance with the |histogram| name to be used during
// actions' execution.
- (instancetype)initWithHistogram:(const char*)histogram;

// Creates a UIAction instance configured with the given |title| and |image|.
// Upon execution, the action's |type| will be recorded and the |block| will be
// run.
- (UIAction*)actionWithTitle:(NSString*)title
                       image:(UIImage*)image
                        type:(MenuActionType)type
                       block:(ProceduralBlock)block;

// Creates a UIAction instance configured to copy the given |URL| to the
// pasteboard.
- (UIAction*)actionToCopyURL:(const GURL)URL;

@end

#endif  // IOS_CHROME_BROWSER_UI_MENU_ACTION_FACTORY_H_
