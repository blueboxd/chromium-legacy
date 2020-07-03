// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"

#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#include "ios/chrome/browser/passwords/password_store_observer_bridge.h"
#import "ios/chrome/browser/passwords/save_passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordsMediator () <PasswordCheckObserver,
                                 PasswordStoreObserver,
                                 SavePasswordsConsumerDelegate> {
  // The service responsible for password check feature.
  IOSChromePasswordCheckManager* _manager;

  // The interface for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStore> _passwordStore;

  // A helper object for passing data about changes in password check status
  // and changes to compromised credentials list.
  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;

  // A helper object for passing data about saved passwords from a finished
  // password store request to the PasswordsTableViewController.
  std::unique_ptr<ios::SavePasswordsConsumer> _savedPasswordsConsumer;

  // A helper object which listens to the password store changes.
  std::unique_ptr<PasswordStoreObserverBridge> _passwordStoreObserver;

  // Current state of password check.
  PasswordCheckState _currentState;
}

@end

@implementation PasswordsMediator

- (instancetype)initWithPasswordStore:
                    (scoped_refptr<password_manager::PasswordStore>)
                        passwordStore
                 passwordCheckManager:(IOSChromePasswordCheckManager*)manager {
  self = [super init];
  if (self) {
    _manager = manager;
    _passwordStore = passwordStore;
    _savedPasswordsConsumer =
        std::make_unique<ios::SavePasswordsConsumer>(self);

    if (base::FeatureList::IsEnabled(
            password_manager::features::kPasswordCheck)) {
      _passwordCheckObserver =
          std::make_unique<PasswordCheckObserverBridge>(self, manager);
      _passwordStoreObserver =
          std::make_unique<PasswordStoreObserverBridge>(self);
      _passwordStore->AddObserver(_passwordStoreObserver.get());
    }
  }
  return self;
}

- (void)dealloc {
  if (_passwordStoreObserver) {
    _passwordStore->RemoveObserver(_passwordStoreObserver.get());
  }
}

- (void)setConsumer:(id<PasswordsConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  [self loginsDidChange];
  [self.consumer setPasswordCheckUIState:
                     [self computePasswordCheckUIStateWithChangedState:NO]];
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  if (state == _currentState)
    return;

  _currentState = state;
  DCHECK(self.consumer);
  [self.consumer setPasswordCheckUIState:
                     [self computePasswordCheckUIStateWithChangedState:YES]];
}

- (void)compromisedCredentialsDidChange:
    (password_manager::CompromisedCredentialsManager::CredentialsView)
        credentials {
  DCHECK(self.consumer);
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
    case PasswordCheckState::kCanceled:
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

#pragma mark - PasswordStoreObserver

- (void)loginsDidChange {
  // Cancel ongoing requests to the password store and issue a new request.
  _savedPasswordsConsumer->cancelable_task_tracker()->TryCancelAll();
  _passwordStore->GetAllLogins(_savedPasswordsConsumer.get());
}

#pragma mark - SavePasswordsConsumerDelegate

- (void)onGetPasswordStoreResults:
    (std::vector<std::unique_ptr<autofill::PasswordForm>>)results {
  DCHECK(self.consumer);
  [self.consumer setPasswordsForms:std::move(results)];
}

@end
