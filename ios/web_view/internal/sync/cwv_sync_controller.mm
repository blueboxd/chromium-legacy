// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"

#import <UIKit/UIKit.h>
#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller_data_source.h"
#import "ios/web_view/public/cwv_sync_controller_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using AccessTokenCallback = DeviceAccountsProvider::AccessTokenCallback;

NSErrorDomain const CWVSyncErrorDomain =
    @"org.chromium.chromewebview.SyncErrorDomain";

namespace {
CWVSyncError CWVConvertGoogleServiceAuthErrorStateToCWVSyncError(
    GoogleServiceAuthError::State state) {
  switch (state) {
    case GoogleServiceAuthError::NONE:
      return CWVSyncErrorNone;
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
      return CWVSyncErrorInvalidGAIACredentials;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
      return CWVSyncErrorUserNotSignedUp;
    case GoogleServiceAuthError::CONNECTION_FAILED:
      return CWVSyncErrorConnectionFailed;
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      return CWVSyncErrorServiceUnavailable;
    case GoogleServiceAuthError::REQUEST_CANCELED:
      return CWVSyncErrorRequestCanceled;
    case GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE:
      return CWVSyncErrorUnexpectedServiceResponse;
    // The following errors are unexpected on iOS.
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::NUM_STATES:
      NOTREACHED();
      return CWVSyncErrorNone;
  }
}
}  // namespace

@interface CWVSyncController ()

// Called by WebViewSyncControllerObserverBridge's
// |OnSyncConfigurationCompleted|.
- (void)didCompleteSyncConfiguration;
// Called by WebViewSyncControllerObserverBridge's |OnSyncShutdown|.
- (void)didShutdownSync;
// Called by WebViewSyncControllerObserverBridge's |OnErrorChanged|.
- (void)didUpdateAuthError;

// Call to refresh access tokens for |currentIdentity|.
- (void)reloadCredentials;

@end

namespace ios_web_view {

// Bridge that observes syncer::SyncService and calls analagous
// methods on CWVSyncController.
class WebViewSyncControllerObserverBridge
    : public syncer::SyncServiceObserver,
      public SigninErrorController::Observer {
 public:
  explicit WebViewSyncControllerObserverBridge(
      CWVSyncController* sync_controller)
      : sync_controller_(sync_controller) {}

  // syncer::SyncServiceObserver:
  void OnSyncConfigurationCompleted(syncer::SyncService* sync) override {
    [sync_controller_ didCompleteSyncConfiguration];
  }

  void OnSyncShutdown(syncer::SyncService* sync) override {
    [sync_controller_ didShutdownSync];
  }

  // SigninErrorController::Observer:
  void OnErrorChanged() override { [sync_controller_ didUpdateAuthError]; }

 private:
  __weak CWVSyncController* sync_controller_;
};

}  // namespace ios_web_view

@implementation CWVSyncController {
  syncer::SyncService* _syncService;
  signin::IdentityManager* _identityManager;
  SigninErrorController* _signinErrorController;
  std::unique_ptr<ios_web_view::WebViewSyncControllerObserverBridge> _observer;

  // Data source that can provide access tokens.
  __weak id<CWVSyncControllerDataSource> _dataSource;
}

@synthesize delegate = _delegate;
@synthesize currentIdentity = _currentIdentity;

- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
                    identityManager:(signin::IdentityManager*)identityManager
              signinErrorController:
                  (SigninErrorController*)signinErrorController {
  self = [super init];
  if (self) {
    _syncService = syncService;
    _identityManager = identityManager;
    _signinErrorController = signinErrorController;
    _observer =
        std::make_unique<ios_web_view::WebViewSyncControllerObserverBridge>(
            self);
    _syncService->AddObserver(_observer.get());
    _signinErrorController->AddObserver(_observer.get());

    // Refresh access tokens on foreground to extend expiration dates.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(reloadCredentials)
               name:UIApplicationWillEnterForegroundNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  _syncService->RemoveObserver(_observer.get());
  _signinErrorController->RemoveObserver(_observer.get());
}

#pragma mark - Public Methods

- (BOOL)isPassphraseNeeded {
  return _syncService->GetUserSettings()->IsPassphraseRequiredForDecryption();
}

- (void)startSyncWithIdentity:(CWVIdentity*)identity
                   dataSource:
                       (__weak id<CWVSyncControllerDataSource>)dataSource {
  DCHECK(!_dataSource);
  DCHECK(!_currentIdentity);

  _dataSource = dataSource;
  _currentIdentity = identity;

  DCHECK(_dataSource);
  DCHECK(_currentIdentity);

  const CoreAccountId accountId = _identityManager->PickAccountIdForAccount(
      base::SysNSStringToUTF8(identity.gaiaID),
      base::SysNSStringToUTF8(identity.email));

  _identityManager->GetDeviceAccountsSynchronizer()
      ->ReloadAllAccountsFromSystem();
  CHECK(_identityManager->HasAccountWithRefreshToken(accountId));

  _identityManager->GetPrimaryAccountMutator()->SetPrimaryAccount(accountId);
  CHECK_EQ(_identityManager->GetPrimaryAccountId(), accountId);
}

- (void)stopSyncAndClearIdentity {
  auto* primaryAccountMutator = _identityManager->GetPrimaryAccountMutator();
  primaryAccountMutator->ClearPrimaryAccount(
      signin::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  _currentIdentity = nil;
  _dataSource = nil;
}

- (BOOL)unlockWithPassphrase:(NSString*)passphrase {
  return _syncService->GetUserSettings()->SetDecryptionPassphrase(
      base::SysNSStringToUTF8(passphrase));
}

#pragma mark - Private Methods

- (void)didCompleteSyncConfiguration {
  if ([_delegate respondsToSelector:@selector(syncControllerDidStartSync:)]) {
    [_delegate syncControllerDidStartSync:self];
  }
}

- (void)didShutdownSync {
  _syncService->RemoveObserver(_observer.get());
  _signinErrorController->RemoveObserver(_observer.get());
}

- (void)reloadCredentials {
  if (_currentIdentity != nil) {
    _identityManager->GetDeviceAccountsSynchronizer()
        ->ReloadAllAccountsFromSystem();
  }
}

#pragma mark - Internal Methods

- (void)fetchAccessTokenForScopes:(const std::set<std::string>&)scopes
                         callback:(AccessTokenCallback)callback {
  DCHECK(!callback.is_null());
  NSMutableArray<NSString*>* scopesArray = [NSMutableArray array];
  for (const auto& scope : scopes) {
    [scopesArray addObject:base::SysUTF8ToNSString(scope)];
  }

  // AccessTokenCallback is non-copyable. Using __block allocates the memory
  // directly in the block object at compilation time (instead of doing a
  // copy). This is required to have correct interaction between move-only
  // types and Objective-C blocks.
  __block AccessTokenCallback scopedCallback = std::move(callback);
  [_dataSource syncController:self
      getAccessTokenForScopes:[scopesArray copy]
            completionHandler:^(NSString* accessToken, NSDate* expirationDate,
                                NSError* error) {
              std::move(scopedCallback).Run(accessToken, expirationDate, error);
            }];
}

- (void)didSignoutWithSourceMetric:(signin_metrics::ProfileSignout)metric {
  if (![_delegate respondsToSelector:@selector
                  (syncController:didStopSyncWithReason:)]) {
    return;
  }
  CWVStopSyncReason reason;
  if (metric == signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS) {
    reason = CWVStopSyncReasonClient;
  } else if (metric == signin_metrics::ProfileSignout::SERVER_FORCED_DISABLE) {
    reason = CWVStopSyncReasonServer;
  } else {
    NOTREACHED();
    return;
  }
  [_delegate syncController:self didStopSyncWithReason:reason];
}

- (void)didUpdateAuthError {
  GoogleServiceAuthError authError = _signinErrorController->auth_error();
  CWVSyncError code =
      CWVConvertGoogleServiceAuthErrorStateToCWVSyncError(authError.state());
  if (code != CWVSyncErrorNone) {
    if ([_delegate
            respondsToSelector:@selector(syncController:didFailWithError:)]) {
      NSError* error =
          [NSError errorWithDomain:CWVSyncErrorDomain code:code userInfo:nil];
      [_delegate syncController:self didFailWithError:error];
    }
  }
}

@end
