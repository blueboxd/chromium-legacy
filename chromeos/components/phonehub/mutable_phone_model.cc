// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/mutable_phone_model.h"

namespace chromeos {
namespace phonehub {

MutablePhoneModel::MutablePhoneModel() = default;

MutablePhoneModel::~MutablePhoneModel() = default;

void MutablePhoneModel::SetPhoneStatusModel(
    const base::Optional<PhoneStatusModel>& phone_status_model) {
  if (phone_status_model_ == phone_status_model)
    return;

  phone_status_model_ = phone_status_model;
  NotifyModelChanged();
}

}  // namespace phonehub
}  // namespace chromeos
