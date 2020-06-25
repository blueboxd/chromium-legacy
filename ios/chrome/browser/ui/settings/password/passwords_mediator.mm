// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"

#include "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordsMediator () <PasswordCheckObserver> {
  // The service responsible for password check feature.
  IOSChromePasswordCheckManager* _manager;

  // A helper object for passing data about changes in password check status
  // and changes to compromised credentials list.
  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;

  // Current state of password check.
  PasswordCheckState _currentState;
}

@property(nonatomic, weak) id<PasswordsConsumer> consumer;

@end

@implementation PasswordsMediator

- (instancetype)initWithConsumer:(id<PasswordsConsumer>)consumer
            passwordCheckManager:(IOSChromePasswordCheckManager*)manager {
  self = [super init];
  if (self) {
    _consumer = consumer;
    _manager = manager;
    _passwordCheckObserver.reset(
        new PasswordCheckObserverBridge(self, manager));
  }
  return self;
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  if (state == _currentState)
    return;

  _currentState = state;
  [self.consumer setPasswordCheckUIState:
                     [self computePasswordCheckUIStateWithChangedState:YES]];
}

- (void)compromisedCredentialsDidChange:
    (password_manager::CompromisedCredentialsManager::CredentialsView)
        credentials {
  [self.consumer setPasswordCheckUIState:
                     [self computePasswordCheckUIStateWithChangedState:NO]];
}

#pragma mark - Private Methods

// Returns PasswordCheckUIState based on PasswordCheckState.
// Parameter indicates whether function called when |_currentState| changed as
// safe status is only possible if state changed from kRunning to kIdle.
- (PasswordCheckUIState)computePasswordCheckUIStateWithChangedState:
    (BOOL)stateChanged {
  switch (_currentState) {
    case PasswordCheckState::kRunning: {
      return PasswordCheckStateRunning;
    }
    case PasswordCheckState::kNoPasswords: {
      return PasswordCheckStateDisabled;
    }
    case PasswordCheckState::kIdle:
    case PasswordCheckState::kSignedOut:
    case PasswordCheckState::kOffline:
    case PasswordCheckState::kQuotaLimit:
    case PasswordCheckState::kOther: {
      if (!_manager->GetCompromisedCredentials().empty()) {
        return PasswordCheckStateUnSafe;
      } else if (_currentState == PasswordCheckState::kIdle) {
        return stateChanged ? PasswordCheckStateSafe
                            : PasswordCheckStateDefault;
      }
      return PasswordCheckStateDefault;
    }
  }
}

@end
