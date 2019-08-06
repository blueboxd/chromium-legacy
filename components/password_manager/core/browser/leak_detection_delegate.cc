// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate.h"

#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_factory_impl.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {

using Logger = autofill::SavePasswordProgressLogger;

LeakDetectionDelegate::LeakDetectionDelegate(PasswordManagerClient* client)
    : client_(client),
      leak_factory_(std::make_unique<LeakDetectionRequestFactoryImpl>()) {}

LeakDetectionDelegate::~LeakDetectionDelegate() = default;

void LeakDetectionDelegate::StartLeakCheck(const autofill::PasswordForm& form) {
  if (client_->IsIncognito())
    return;
  leak_check_ = leak_factory_->TryCreateLeakCheck(
      this, client_->GetIdentityManager(), client_->GetURLLoaderFactory());
  if (leak_check_)
    leak_check_->Start(form.origin, form.username_value, form.password_value);
}

void LeakDetectionDelegate::OnLeakDetectionDone(bool leaked,
                                                const GURL& url,
                                                base::StringPiece16 username) {
  leak_check_.reset();
  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogBoolean(Logger::STRING_LEAK_DETECTION_FINISHED, leaked);
  }
}

void LeakDetectionDelegate::OnError(LeakDetectionError error) {
  leak_check_.reset();

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    switch (error) {
      case LeakDetectionError::kNotSignIn:
        logger.LogMessage(Logger::STRING_LEAK_DETECTION_SIGNED_OUT_ERROR);
        break;
    }
  }
}

}  // namespace password_manager
