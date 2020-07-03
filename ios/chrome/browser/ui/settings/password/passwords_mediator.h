// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#include "base/memory/scoped_refptr.h"

class IOSChromePasswordCheckManager;
@protocol PasswordsConsumer;

namespace password_manager {
class PasswordStore;
}

// This mediator fetches and organises the passwords for its consumer.
@interface PasswordsMediator : NSObject

- (instancetype)initWithPasswordStore:
                    (scoped_refptr<password_manager::PasswordStore>)
                        passwordStore
                 passwordCheckManager:(IOSChromePasswordCheckManager*)manager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<PasswordsConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_H_
