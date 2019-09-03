// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_update_delegate.h"

PasswordUpdateDelegate::PasswordUpdateDelegate(
    password_manager::PasswordStore* store,
    const autofill::PasswordForm& password_form)
    : password_form_(password_form) {}

PasswordUpdateDelegate::~PasswordUpdateDelegate() = default;
