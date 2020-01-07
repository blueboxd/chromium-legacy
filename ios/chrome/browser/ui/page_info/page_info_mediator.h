// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_MEDIATOR_H_

#import <Foundation/Foundation.h>

namespace web {
struct SSLStatus;
}

@class PageInfoConfig;
class GURL;

// Mediator for the PageInfo, extracting the data to be displayed for the web
// informations.
@interface PageInfoMediator : NSObject

// For now this object only have static method.
- (instancetype)init NS_UNAVAILABLE;

// Returns a configuration based on the |URL|, the SSL |status| and if the
// current page is an |offlinePage|.
+ (PageInfoConfig*)configurationForURL:(const GURL&)URL
                             SSLStatus:(web::SSLStatus&)status
                           offlinePage:(BOOL)offlinePage;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_MEDIATOR_H_
