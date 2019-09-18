// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/debug_logs_manager.h"

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user.h"

namespace {
const char kSupportedEmailSuffix[] = "@google.com";
}  // namespace

DebugLogsManager::DebugLogsManager(const user_manager::User* primary_user)
    : primary_user_(primary_user) {}

DebugLogsManager::~DebugLogsManager() = default;

DebugLogsManager::DebugLogsState DebugLogsManager::GetDebugLogsState() const {
  if (!AreDebugLogsSupported())
    return DebugLogsState::kNotSupported;

  return are_debug_logs_enabled_ ? DebugLogsState::kSupportedAndEnabled
                                 : DebugLogsState::kSupportedButDisabled;
}

mojom::DebugLogsChangeHandlerPtr DebugLogsManager::GenerateInterfacePtr() {
  mojom::DebugLogsChangeHandlerPtr interface_ptr;
  bindings_.AddBinding(this, mojo::MakeRequest(&interface_ptr));
  return interface_ptr;
}

void DebugLogsManager::ChangeDebugLogsState(bool should_debug_logs_be_enabled) {
  DCHECK_NE(GetDebugLogsState(), DebugLogsState::kNotSupported);

  // TODO(yshavit): Handle the user enabling/disabling logs.
  are_debug_logs_enabled_ = should_debug_logs_be_enabled;
}

bool DebugLogsManager::AreDebugLogsSupported() const {
  if (!base::FeatureList::IsEnabled(
          chromeos::features::kShowBluetoothDebugLogToggle)) {
    return false;
  }

  if (!primary_user_)
    return false;

  return base::EndsWith(primary_user_->GetDisplayEmail(), kSupportedEmailSuffix,
                        base::CompareCase::INSENSITIVE_ASCII);
}
