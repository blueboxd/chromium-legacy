// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_check/android/bulk_leak_check_controller_android.h"

BulkLeakCheckControllerAndroid::BulkLeakCheckControllerAndroid() = default;

BulkLeakCheckControllerAndroid::~BulkLeakCheckControllerAndroid() = default;

void BulkLeakCheckControllerAndroid::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void BulkLeakCheckControllerAndroid::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void BulkLeakCheckControllerAndroid::StartPasswordCheck() {
  // TODO(crbug.com/1092444): connect with the actual passwords check logic.
  for (Observer& obs : observers_) {
    obs.OnStateChanged(password_manager::BulkLeakCheckService::State::kIdle);
  }
}

int BulkLeakCheckControllerAndroid::GetNumberOfSavedPasswords() {
  // TODO(crbug.com/1092444): connect with the actual passwords check logic.
  return 0;
}

int BulkLeakCheckControllerAndroid::GetNumberOfLeaksFromLastCheck() {
  // TODO(crbug.com/1092444): connect with the actual passwords check logic.
  return 0;
}
