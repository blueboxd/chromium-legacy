// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_CWV_SYNC_CONTROLLER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_CWV_SYNC_CONTROLLER_INTERNAL_H_

#include <set>

#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ios/web_view/internal/signin/web_view_device_accounts_provider_impl.h"
#import "ios/web_view/public/cwv_sync_controller.h"

NS_ASSUME_NONNULL_BEGIN

namespace syncer {
class SyncService;
}  // namespace syncer

namespace signin {
class IdentityManager;
}

class SigninErrorController;

@interface CWVSyncController ()

// All dependencies must out live this class.
- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
                    identityManager:(signin::IdentityManager*)identityManager
              signinErrorController:
                  (SigninErrorController*)SigninErrorController
    NS_DESIGNATED_INITIALIZER;

// Called by WebViewDeviceAccountsProviderImpl to obtain
// access tokens for |scopes| to be passed back in |callback|.
- (void)fetchAccessTokenForScopes:(const std::set<std::string>&)scopes
                         callback:(DeviceAccountsProvider::AccessTokenCallback)
                                      callback;

// Called by IOSWebViewSigninClient when signing out.
- (void)didSignoutWithSourceMetric:(signin_metrics::ProfileSignout)metric;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_CWV_SYNC_CONTROLLER_INTERNAL_H_
