// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_IOS_FACTORY_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_IOS_FACTORY_H_

#import <Foundation/Foundation.h>

class WebStateList;
@class SessionIOS;

// A factory that is used to create a SessionIOS object for a specific
// WebStateList. It's the responsibility of the owner of the SessionIOSFactory
// object to provide a WebStateList with a longer life time than this object.
@interface SessionIOSFactory : NSObject

// Initialize the factory with a valid WebStateList.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList;

// Creates a sessionIOS object with a serialized webStateList. This method can't
// be used without initializing the object with a non-null WebStateList.
- (SessionIOS*)sessionForSaving;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_IOS_FACTORY_H_
