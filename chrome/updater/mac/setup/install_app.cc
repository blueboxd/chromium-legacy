// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/setup/install_app.h"

#include "base/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/mac/setup/setup.h"

namespace updater {

namespace {

class AppInstall : public App {
 private:
  ~AppInstall() override = default;
  void FirstTaskRun() override;
};

void AppInstall::FirstTaskRun() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&SetupUpdater),
      base::BindOnce(&AppInstall::Shutdown, this));
}

}  // namespace

scoped_refptr<App> AppInstallInstance() {
  return AppInstance<AppInstall>();
}

}  // namespace updater
