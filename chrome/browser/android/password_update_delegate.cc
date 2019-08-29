// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_update_delegate.h"
#include "chrome/browser/android/password_editing_bridge.h"

PasswordUpdateDelegate::PasswordUpdateDelegate(
    password_manager::PasswordStore* store,
    const autofill::PasswordForm& password_form)
    : password_form_(password_form) {}

PasswordUpdateDelegate::~PasswordUpdateDelegate() = default;

void LaunchPasswordEntryEditor(password_manager::PasswordStore* store,
                               const autofill::PasswordForm& password_form) {
  // PasswordEditingBridge will destroy itself when the UI is gone on the Java
  // side.
  PasswordEditingBridge* password_editing_bridge = new PasswordEditingBridge();
  password_editing_bridge->SetDelegate(
      std::make_unique<PasswordUpdateDelegate>(store, password_form));
}
