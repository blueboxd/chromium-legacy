// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/setup/app_swap.h"

#include "base/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/mac/setup/setup.h"

namespace updater {

namespace {

class AppPromoteCandidate : public App {
 private:
  ~AppPromoteCandidate() override = default;
  void FirstTaskRun() override;
};

void AppPromoteCandidate::FirstTaskRun() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&PromoteCandidate),
      base::BindOnce(&AppPromoteCandidate::Shutdown, this));
}

class AppUninstallCandidate : public App {
 private:
  ~AppUninstallCandidate() override = default;
  void FirstTaskRun() override;
};

void AppUninstallCandidate::FirstTaskRun() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&UninstallCandidate),
      base::BindOnce(&AppUninstallCandidate::Shutdown, this));
}

}  // namespace

scoped_refptr<App> AppPromoteCandidateInstance() {
  return AppInstance<AppPromoteCandidate>();
}

scoped_refptr<App> AppUninstallCandidateInstance() {
  return AppInstance<AppUninstallCandidate>();
}

}  // namespace updater
