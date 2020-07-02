// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues_mediator.h"

#include "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/password/password_issue_with_form.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordIssuesMediator () <PasswordCheckObserver> {
  IOSChromePasswordCheckManager* _manager;

  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;

  std::vector<password_manager::CredentialWithPassword> _compromisedCredentials;
}

@end

@implementation PasswordIssuesMediator

- (instancetype)initWithPasswordCheckManager:
    (IOSChromePasswordCheckManager*)manager {
  self = [super init];
  if (self) {
    _manager = manager;
    _passwordCheckObserver.reset(
        new PasswordCheckObserverBridge(self, manager));
  }
  return self;
}

- (void)setConsumer:(id<PasswordIssuesConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  [self fetchPasswordIssues];
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  // No-op.
}

- (void)compromisedCredentialsDidChange:
    (password_manager::CompromisedCredentialsManager::CredentialsView)
        credentials {
  [self fetchPasswordIssues];
}

#pragma mark - Private Methods

- (void)fetchPasswordIssues {
  DCHECK(self.consumer);
  _compromisedCredentials = _manager->GetCompromisedCredentials();
  NSMutableArray* passwords = [[NSMutableArray alloc] init];
  for (auto credential : _compromisedCredentials) {
    const autofill::PasswordForm form =
        _manager->GetSavedPasswordsFor(credential)[0];
    [passwords
        addObject:[[PasswordIssueWithForm alloc] initWithPasswordForm:form]];
  }
  [self.consumer setPasswordIssues:passwords];
}

@end
