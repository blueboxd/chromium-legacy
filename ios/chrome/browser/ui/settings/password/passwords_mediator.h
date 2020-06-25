// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_H_

#import <Foundation/Foundation.h>

class IOSChromePasswordCheckManager;
@protocol PasswordsConsumer;

// This mediator fetches and organises the passwords for its consumer.
@interface PasswordsMediator : NSObject

- (instancetype)initWithConsumer:(id<PasswordsConsumer>)consumer
            passwordCheckManager:(IOSChromePasswordCheckManager*)manager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_H_
