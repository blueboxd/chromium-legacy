// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/test_app/update_client_win.h"

namespace updater {

UpdateClientWin::UpdateClientWin() = default;

UpdateClientWin::~UpdateClientWin() = default;

bool UpdateClientWin::CanDialIPC() {
  return true;
}

void UpdateClientWin::BeginRegister(const std::string& brand_code,
                                    const std::string& tag,
                                    const std::string& version,
                                    UpdateService::Callback callback) {
  // TODO(1068693): Implement TestApp Functionality.
  NOTIMPLEMENTED();
}

void UpdateClientWin::BeginUpdateCheck(
    UpdateService::StateChangeCallback state_change,
    UpdateService::Callback callback) {
  // TODO(1068693): Implement TestApp Functionality.
  NOTIMPLEMENTED();
}

scoped_refptr<UpdateClient> UpdateClient::Create() {
  return base::MakeRefCounted<UpdateClientWin>();
}

}  // namespace updater
