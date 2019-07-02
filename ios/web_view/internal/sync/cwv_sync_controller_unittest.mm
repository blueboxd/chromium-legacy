// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"

#include <memory>
#include <set>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/test/bind_test_util.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/ios/browser/fake_device_accounts_provider.h"
#include "components/sync/driver/mock_sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller_data_source.h"
#import "ios/web_view/public/cwv_sync_controller_delegate.h"
#include "ios/web_view/test/test_with_locale_and_resources.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {
namespace {

using testing::_;
using testing::Invoke;
using testing::Return;

const char kTestGaiaId[] = "1337";
const char kTestEmail[] = "johndoe@chromium.org";
const char kTestFullName[] = "John Doe";
const char kTestPassphrase[] = "dummy-passphrase";
const char kTestScope1[] = "scope1.chromium.org";
const char kTestScope2[] = "scope2.chromium.org";

}  // namespace

class CWVSyncControllerTest : public TestWithLocaleAndResources {
 protected:
  CWVSyncControllerTest()
      : browser_state_(/*off_the_record=*/false),
        identity_test_env_(nullptr,
                           nullptr,
                           signin::AccountConsistencyMethod::kDisabled,
                           nullptr,
                           CreateExtraParams()),
        signin_error_controller_(
            SigninErrorController::AccountMode::ANY_ACCOUNT,
            identity_test_env_.identity_manager()) {
    web_state_.SetBrowserState(&browser_state_);

    profile_sync_service_ = std::make_unique<syncer::MockSyncService>();

    EXPECT_CALL(*profile_sync_service_, AddObserver(_))
        .WillOnce(Invoke(this, &CWVSyncControllerTest::AddObserver));

    sync_controller_ = [[CWVSyncController alloc]
          initWithSyncService:profile_sync_service_.get()
              identityManager:identity_test_env_.identity_manager()
        signinErrorController:&signin_error_controller_];
  }

  ~CWVSyncControllerTest() override {
    EXPECT_CALL(*profile_sync_service_, RemoveObserver(_));
  }

  void AddObserver(syncer::SyncServiceObserver* observer) {
    sync_service_observer_ = observer;
  }

  static identity::IdentityTestEnvironment::ExtraParams CreateExtraParams() {
    auto provider = std::make_unique<FakeDeviceAccountsProvider>();
    provider->AddAccount(kTestGaiaId, kTestEmail);
    identity::IdentityTestEnvironment::ExtraParams extra_params;
    extra_params.token_service_provider = std::move(provider);
    return extra_params;
  }

  web::TestWebThreadBundle web_thread_bundle_;
  ios_web_view::WebViewBrowserState browser_state_;
  web::TestWebState web_state_;
  identity::IdentityTestEnvironment identity_test_env_;
  SigninErrorController signin_error_controller_;
  std::unique_ptr<syncer::MockSyncService> profile_sync_service_;
  CWVSyncController* sync_controller_;
  syncer::SyncServiceObserver* sync_service_observer_;
};

// Verifies CWVSyncControllerDataSource methods are invoked with the correct
// parameters.
TEST_F(CWVSyncControllerTest, DataSourceCallbacks) {
  // [data_source expect] returns an autoreleased object, but it must be
  // destroyed before this test exits to avoid holding on to |sync_controller_|.
  @autoreleasepool {
    id data_source = OCMProtocolMock(@protocol(CWVSyncControllerDataSource));

    [[data_source expect]
                 syncController:sync_controller_
        getAccessTokenForScopes:[OCMArg checkWithBlock:^BOOL(NSArray* scopes) {
          return [scopes containsObject:@(kTestScope1)] &&
                 [scopes containsObject:@(kTestScope2)];
        }]
              completionHandler:[OCMArg any]];

    CWVIdentity* identity = [[CWVIdentity alloc] initWithEmail:@(kTestEmail)
                                                      fullName:@(kTestFullName)
                                                        gaiaID:@(kTestGaiaId)];
    [sync_controller_ startSyncWithIdentity:identity dataSource:data_source];

    std::set<std::string> scopes = {kTestScope1, kTestScope2};
    [sync_controller_ fetchAccessTokenForScopes:scopes
                                       callback:base::DoNothing()];

    [data_source verify];
  }
}

// Verifies CWVSyncControllerDelegate methods are invoked with the correct
// parameters.
TEST_F(CWVSyncControllerTest, DelegateCallbacks) {
  // [delegate expect] returns an autoreleased object, but it must be destroyed
  // before this test exits to avoid holding on to |sync_controller_|.
  @autoreleasepool {
    id delegate = OCMProtocolMock(@protocol(CWVSyncControllerDelegate));
    sync_controller_.delegate = delegate;

    [[delegate expect] syncControllerDidStartSync:sync_controller_];
    sync_service_observer_->OnSyncConfigurationCompleted(
        profile_sync_service_.get());
    [[delegate expect]
          syncController:sync_controller_
        didFailWithError:[OCMArg checkWithBlock:^BOOL(NSError* error) {
          return error.code == CWVSyncErrorInvalidGAIACredentials;
        }]];

    id data_source = OCMProtocolMock(@protocol(CWVSyncControllerDataSource));

    CWVIdentity* identity = [[CWVIdentity alloc] initWithEmail:@(kTestEmail)
                                                      fullName:@(kTestFullName)
                                                        gaiaID:@(kTestGaiaId)];
    [sync_controller_ startSyncWithIdentity:identity dataSource:data_source];

    // Create authentication error.
    GoogleServiceAuthError auth_error(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
    identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
        identity_test_env_.identity_manager()->GetPrimaryAccountId(),
        auth_error);

    [[delegate expect] syncController:sync_controller_
                didStopSyncWithReason:CWVStopSyncReasonServer];
    [sync_controller_
        didSignoutWithSourceMetric:signin_metrics::ProfileSignout::
                                       SERVER_FORCED_DISABLE];
    [delegate verify];
  }
}

// Verifies CWVSyncController properly maintains the current syncing user.
TEST_F(CWVSyncControllerTest, CurrentIdentity) {
  CWVIdentity* identity = [[CWVIdentity alloc] initWithEmail:@(kTestEmail)
                                                    fullName:@(kTestFullName)
                                                      gaiaID:@(kTestGaiaId)];
  id unused_mock = OCMProtocolMock(@protocol(CWVSyncControllerDataSource));
  [sync_controller_ startSyncWithIdentity:identity dataSource:unused_mock];
  CWVIdentity* currentIdentity = sync_controller_.currentIdentity;
  EXPECT_TRUE(currentIdentity);
  EXPECT_NSEQ(identity.email, currentIdentity.email);
  EXPECT_NSEQ(identity.fullName, currentIdentity.fullName);
  EXPECT_NSEQ(identity.gaiaID, currentIdentity.gaiaID);

  [sync_controller_ stopSyncAndClearIdentity];
  EXPECT_FALSE(sync_controller_.currentIdentity);
}

// Verifies CWVSyncController's passphrase API.
TEST_F(CWVSyncControllerTest, Passphrase) {
  EXPECT_CALL(*profile_sync_service_->GetMockUserSettings(),
              IsPassphraseRequiredForDecryption())
      .WillOnce(Return(true));
  EXPECT_TRUE(sync_controller_.passphraseNeeded);
  EXPECT_CALL(*profile_sync_service_->GetMockUserSettings(),
              SetDecryptionPassphrase(kTestPassphrase))
      .WillOnce(Return(true));
  EXPECT_TRUE([sync_controller_ unlockWithPassphrase:@(kTestPassphrase)]);
}

}  // namespace ios_web_view
