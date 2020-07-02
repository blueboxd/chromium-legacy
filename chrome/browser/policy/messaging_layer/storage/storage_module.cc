// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback.h"
#include "base/no_destructor.h"
#include "chrome/browser/policy/messaging_layer/storage/storage_module.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"

namespace reporting {

StorageModule::StorageModule() = default;

StorageModule::~StorageModule() = default;

void StorageModule::AddRecord(reporting::EncryptedRecord record,
                              reporting::Priority priority,
                              base::OnceCallback<void(Status)> callback) {
  std::move(callback).Run(
      Status(error::UNIMPLEMENTED, "AddRecord isn't implemented"));
}

// static
StatusOr<scoped_refptr<StorageModule>> StorageModule::Create() {
  scoped_refptr<StorageModule> instance =
      // Cannot base::MakeRefCounted, since constructor is protected.
      base::WrapRefCounted(new StorageModule());
  return instance;
}

}  // namespace reporting
