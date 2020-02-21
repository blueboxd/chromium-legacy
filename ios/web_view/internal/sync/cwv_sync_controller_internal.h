// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_CWV_SYNC_CONTROLLER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_CWV_SYNC_CONTROLLER_INTERNAL_H_

#import "ios/web_view/public/cwv_sync_controller.h"

NS_ASSUME_NONNULL_BEGIN

namespace autofill {
class AutofillWebDataService;
class PersonalDataManager;
}  // autofill

namespace syncer {
class SyncService;
}  // namespace syncer

namespace signin {
class IdentityManager;
}  // namespace signin

namespace password_manager {
class PasswordStore;
}  // password_manager

class SigninErrorController;

@interface CWVSyncController ()

// All dependencies must out live this class.
- (instancetype)
       initWithSyncService:(syncer::SyncService*)syncService
           identityManager:(signin::IdentityManager*)identityManager
     signinErrorController:(SigninErrorController*)signinErrorController
       personalDataManager:(autofill::PersonalDataManager*)personalDataManager
    autofillWebDataService:
        (autofill::AutofillWebDataService*)autofillWebDataService
             passwordStore:(password_manager::PasswordStore*)passwordStore
    NS_DESIGNATED_INITIALIZER;

// Called by the associated CWVWebViewConfiguration in order to shut
// down cleanly. See CWVWebViewConfiguration's |shutDown| method for more info.
- (void)shutDown;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_CWV_SYNC_CONTROLLER_INTERNAL_H_
