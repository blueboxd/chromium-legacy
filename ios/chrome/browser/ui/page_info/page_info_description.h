// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_DESCRIPTION_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_DESCRIPTION_H_

#import <UIKit/UIKit.h>

class GURL;

namespace web {
struct SSLStatus;
}

// Create and store all the data to be displayed inside the root of the page
// info.
@interface PageInfoDescription : NSObject

// Init based on the |URL|, the SSL |status| and if the current page is an
// |offlinePage|.
- (instancetype)initWithURL:(const GURL&)URL
                  SSLStatus:(const web::SSLStatus&)status
              isPageOffline:(BOOL)isOffline NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Describes the security status of this page, i.e. "Page is secure" or "SSL
// cert expired".
@property(nonatomic, readonly) NSString* pageSecurityStatusDescription;

// The icon name for the page security status, e.g. a lock.
@property(nonatomic, readonly) NSString* pageSecurityStatusIconImageName;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_DESCRIPTION_H_
