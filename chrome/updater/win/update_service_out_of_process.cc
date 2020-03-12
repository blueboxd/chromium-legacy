// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/update_service_out_of_process.h"

#include "base/callback.h"
#include "base/logging.h"

namespace updater {

UpdateServiceOutOfProcess::UpdateServiceOutOfProcess() = default;

UpdateServiceOutOfProcess::~UpdateServiceOutOfProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UpdateServiceOutOfProcess::RegisterApp(
    const RegistrationRequest& request,
    base::OnceCallback<void(const RegistrationResponse&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(sorin) the updater must be run with "--single-process" until
  // crbug.com/1053729 is resolved.
  NOTREACHED();
}

void UpdateServiceOutOfProcess::UpdateAll(
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(sorin) the updater must be run with "--single-process" until
  // crbug.com/1053729 is resolved.
  NOTREACHED();
}

void UpdateServiceOutOfProcess::Update(const std::string& app_id,
                                       UpdateService::Priority priority,
                                       StateChangeCallback state_update,
                                       base::OnceCallback<void(Result)> done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(sorin) the updater must be run with "--single-process" until
  // crbug.com/1053729 is resolved.
  NOTREACHED();
}

}  // namespace updater
