// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WINDOW_ACTIVITIES_WINDOW_ACTIVITY_HELPERS_H_
#define IOS_CHROME_BROWSER_WINDOW_ACTIVITIES_WINDOW_ACTIVITY_HELPERS_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/navigation/referrer.h"
#import "url/gurl.h"

struct UrlLoadParams;

// Helper functions to create NSUserActivity instances that encode specific
// actions in the browser, and to decode those actions from those activities.

// Create a new activity that opens a new, empty tab. |in_incognito| indicates
// if the new tab should be incognito.
NSUserActivity* ActivityToOpenNewTab(bool in_incognito);

// Create a new activity that opens a new tab, loading |url| with the referrer
// |referrer|. |in_incognito| indicates if the new tab should be incognito.
NSUserActivity* ActivityToLoadURL(const GURL& url,
                                  const web::Referrer& referrer,
                                  bool in_incognito);

// true if |activity| is one that indicates a URL load (including loading the
// new tab page in a new tab).
bool ActivityIsURLLoad(NSUserActivity* activity);

// The URLLoadParams needed to perform the load defined in |activity|, if any.
// If |activity| is not a URL load activity, the default UrlLoadParams are
// returned.
UrlLoadParams LoadParamsFromActivity(NSUserActivity* activity);

#endif  // IOS_CHROME_BROWSER_WINDOW_ACTIVITIES_WINDOW_ACTIVITY_HELPERS_H_
