// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/proto/record_constants.pb.h"

namespace reporting {

using policy::DMToken;
using reporting_messaging_layer::Destination;
using reporting_messaging_layer::Priority;

StatusOr<std::unique_ptr<ReportQueueConfiguration>>
ReportQueueConfiguration::Create(const policy::DMToken& dm_token,
                                 Destination destination,
                                 Priority priority) {
  auto config = base::WrapUnique<ReportQueueConfiguration>(
      new ReportQueueConfiguration());
  RETURN_IF_ERROR(config->SetDMToken(dm_token));
  RETURN_IF_ERROR(config->SetDestination(destination));
  RETURN_IF_ERROR(config->SetPriority(priority));
  return config;
}

Status ReportQueueConfiguration::SetDMToken(const DMToken& dm_token) {
  if (!dm_token.is_valid()) {
    return Status(error::INVALID_ARGUMENT, "DMToken must be valid");
  }
  dm_token_ = dm_token;
  return Status::StatusOK();
}

Status ReportQueueConfiguration::SetDestination(Destination destination) {
  if (destination == Destination::UNDEFINED_DESTINATION) {
    return Status(error::INVALID_ARGUMENT, "Destination must be defined");
  }
  destination_ = destination;
  return Status::StatusOK();
}

Status ReportQueueConfiguration::SetPriority(Priority priority) {
  if (priority == Priority::UNDEFINED_PRIORITY) {
    return Status(error::INVALID_ARGUMENT, "Priority must be defined");
  }
  priority_ = priority;
  return Status::StatusOK();
}

}  // namespace reporting
