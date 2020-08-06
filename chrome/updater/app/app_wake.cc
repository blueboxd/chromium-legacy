// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_wake.h"

#include "base/bind.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/control_service.h"

namespace updater {

// TODO(sorin): Implement the control service for Windows. crbug.com/1105589.
#if !defined(OS_WIN)

// AppWake is a simple client which dials the same-versioned server via RPC and
// tells that server to run its control tasks. This is done via the
// ControlService interface.
class AppWake : public App {
 public:
  AppWake() = default;

 private:
  ~AppWake() override = default;

  // Overrides for App.
  void FirstTaskRun() override;

  scoped_refptr<ControlService> service_;
};

void AppWake::FirstTaskRun() {
  // The service creation might need task runners and the control service needs
  // to be instantiated after the base class has initialized the thread pool.
  //
  // TODO(crbug.com/1113448) - consider initializing the thread pool in the
  // constructor of the base class or earlier, in the updater main.
  service_ = CreateControlService();
  service_->Run(base::BindOnce(&AppWake::Shutdown, this, 0));
}

scoped_refptr<App> MakeAppWake() {
  return base::MakeRefCounted<AppWake>();
}

#endif  // OS_WIN

}  // namespace updater
