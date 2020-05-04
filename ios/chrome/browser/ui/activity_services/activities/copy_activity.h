// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_COPY_ACTIVITY_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_COPY_ACTIVITY_H_

#import <UIKit/UIKit.h>

class GURL;

// Activity that copies the URL to the pasteboard.
@interface CopyActivity : UIActivity

// Initializes the copy activity with the given |URL| to be copied.
- (instancetype)initWithURL:(const GURL&)URL;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_COPY_ACTIVITY_H_
