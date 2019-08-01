// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate.h"

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_factory_impl.h"

namespace password_manager {

LeakDetectionDelegate::LeakDetectionDelegate()
    : leak_factory_(std::make_unique<LeakDetectionRequestFactoryImpl>()) {}

LeakDetectionDelegate::~LeakDetectionDelegate() = default;

void LeakDetectionDelegate::StartLeakCheck(const autofill::PasswordForm& form) {
  leak_check_ = leak_factory_->TryCreateLeakCheck();
  if (leak_check_)
    leak_check_->Start(form.origin, form.username_value, form.password_value);
}

}  // namespace password_manager
