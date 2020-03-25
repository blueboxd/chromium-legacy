// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_uninstall.h"

#include "base/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/updater/app/app.h"

#if defined(OS_WIN)
#include "chrome/updater/win/setup/uninstall.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/updater/mac/setup/setup.h"
#endif

namespace updater {

// AppUninstall uninstalls the updater.
class AppUninstall : public App {
 public:
  AppUninstall() = default;

 private:
  ~AppUninstall() override = default;
  void FirstTaskRun() override;
};

void AppUninstall::FirstTaskRun() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&Uninstall, false),
      base::BindOnce(&AppUninstall::Shutdown, this));
}

scoped_refptr<App> AppUninstallInstance() {
  return AppInstance<AppUninstall>();
}

}  // namespace updater
