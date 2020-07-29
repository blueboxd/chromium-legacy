// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CONTROL_SERVICE_IN_PROCESS_H_
#define CHROME_UPDATER_CONTROL_SERVICE_IN_PROCESS_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/control_service.h"

namespace base {
class SequencedTaskRunner;
}

namespace updater {

class Configurator;

// All functions and callbacks must be called on the same sequence.
class ControlServiceInProcess : public ControlService {
 public:
  explicit ControlServiceInProcess(scoped_refptr<updater::Configurator> config);

  // Overrides for updater::ControlService.
  void Run(base::OnceClosure callback) override;

  void Uninitialize() override;

 private:
  ~ControlServiceInProcess() override;

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<updater::Configurator> config_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_CONTROL_SERVICE_IN_PROCESS_H_
