// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/policy/messaging_layer/encryption/encryption_module.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"
#include "chrome/browser/policy/messaging_layer/storage/storage_module.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"

namespace reporting {

using base::MakeRefCounted;

ReportingClient::ReportingClient()
    : storage_(MakeRefCounted<StorageModule>()),
      encryption_(MakeRefCounted<EncryptionModule>()) {}

ReportingClient::~ReportingClient() = default;

StatusOr<std::unique_ptr<ReportQueue>> ReportingClient::CreateReportQueue(
    std::unique_ptr<ReportQueueConfiguration> config) {
  ASSIGN_OR_RETURN(ReportingClient* instance, GetInstance());
  return ReportQueue::Create(std::move(config), instance->storage_,
                             instance->encryption_);
}

StatusOr<ReportingClient*> ReportingClient::GetInstance() {
  static base::NoDestructor<StatusOr<std::unique_ptr<ReportingClient>>>
      instance(Create());
  if (!instance->ok()) {
    return instance->status();
  }
  return instance->ValueOrDie().get();
}

// TODO(chromium:1078512) As part of completing the StorageModule and
// EncryptionModule, this create function will need to be updated to check for
// successful creation of the StorageModule and EncryptionModule.
StatusOr<std::unique_ptr<ReportingClient>> ReportingClient::Create() {
  auto client = base::WrapUnique<ReportingClient>(new ReportingClient);
  return client;
}

}  // namespace reporting
