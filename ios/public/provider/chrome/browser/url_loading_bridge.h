// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_URL_LOADING_BRIDGE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_URL_LOADING_BRIDGE_H_

#import "url/gurl.h"

// A temporary bridge protocol to change the class used to load URLs.
@protocol URLLoadingBridge <NSObject>

// Asks the implementer to load |URL| in response to a tappable logo.
- (void)loadLogoURL:(GURL)URL;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_URL_LOADING_BRIDGE_H_
