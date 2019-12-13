// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/biometric_authenticator_android.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/biometric_authenticator.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using password_manager::BiometricsAvailability;
using password_manager::UiCredential;

// static
std::unique_ptr<ChromeBiometricAuthenticator>
ChromeBiometricAuthenticator::Create() {
  if (!base::FeatureList::IsEnabled(autofill::features::kAutofillTouchToFill))
    return nullptr;

  if (!base::FeatureList::IsEnabled(
          password_manager::features::kBiometricTouchToFill)) {
    return nullptr;
  }

  return std::make_unique<BiometicAuthenticatorAndroid>();
}

BiometricsAvailability BiometicAuthenticatorAndroid::CanAuthenticate() {
  // TODO(crbug.com/1031483): Implement.
  return BiometricsAvailability::kAvailable;
}

void BiometicAuthenticatorAndroid::Authenticate(const UiCredential& credential,
                                                AuthenticateCallback callback) {
  // TODO(crbug.com/1031483): Implement.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(std::move(callback), true));
}
