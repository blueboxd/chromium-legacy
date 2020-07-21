// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/control_service_in_process.h"

#include "base/callback.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace updater {

ControlServiceInProcess::ControlServiceInProcess()
    : main_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

void ControlServiceInProcess::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/1107586): Implement.
}

void ControlServiceInProcess::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ControlServiceInProcess::~ControlServiceInProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace updater
