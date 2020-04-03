// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/credential_provider/credential_provider_service.h"

#include "base/logging.h"
#include "base/scoped_observer.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"

CredentialProviderService::CredentialProviderService(
    scoped_refptr<password_manager::PasswordStore> password_store)
    : password_store_(password_store) {}

CredentialProviderService::~CredentialProviderService() {}

void CredentialProviderService::Shutdown() {}
