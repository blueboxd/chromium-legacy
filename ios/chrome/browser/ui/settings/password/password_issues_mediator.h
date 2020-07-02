// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_MEDIATOR_H_

#import <Foundation/Foundation.h>

class IOSChromePasswordCheckManager;
@protocol PasswordIssuesConsumer;

// This mediator fetches and organises the credentials for its consumer.
@interface PasswordIssuesMediator : NSObject

- (instancetype)initWithPasswordCheckManager:
    (IOSChromePasswordCheckManager*)manager NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<PasswordIssuesConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_MEDIATOR_H_
