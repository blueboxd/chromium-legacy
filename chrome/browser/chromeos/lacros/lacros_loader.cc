// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lacros/lacros_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"

using component_updater::CrOSComponentManager;

namespace {

LacrosLoader* g_instance = nullptr;

// Some account types require features that aren't yet supported by lacros.
// See https://crbug.com/1080693
bool IsLacrosAllowed() {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user)
    return false;
  switch (user->GetType()) {
    case user_manager::USER_TYPE_REGULAR:
      return true;
    case user_manager::USER_TYPE_GUEST:
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
    case user_manager::USER_TYPE_SUPERVISED:
    case user_manager::USER_TYPE_KIOSK_APP:
    case user_manager::USER_TYPE_CHILD:
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
    case user_manager::NUM_USER_TYPES:
      return false;
  }
}

const char kLacrosComponentName[] = "lacros-fishfood";
const char kUserDataDir[] = "/home/chronos/user/lacros";

bool CheckIfPreviouslyInstalled(
    scoped_refptr<component_updater::CrOSComponentManager> manager) {
  if (!manager->IsRegisteredMayBlock(kLacrosComponentName))
    return false;

  // Since we're already on a background thread, delete the user-data-dir
  // associated with lacros.
  base::DeleteFileRecursively(base::FilePath(kUserDataDir));
  return true;
}
}  // namespace

// static
LacrosLoader* LacrosLoader::Get() {
  return g_instance;
}

LacrosLoader::LacrosLoader(scoped_refptr<CrOSComponentManager> manager)
    : cros_component_manager_(manager) {
  DCHECK(cros_component_manager_);

  DCHECK(!g_instance);
  g_instance = this;
}

LacrosLoader::~LacrosLoader() {
  // Try to kill the lacros-chrome binary.
  if (lacros_process_.IsValid())
    lacros_process_.Terminate(/*ignored=*/0, /*wait=*/false);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void LacrosLoader::Init() {
  if (!IsLacrosAllowed())
    return;

  if (chromeos::features::IsLacrosComponentUpdaterEnabled()) {
    // TODO(crbug.com/1078607): Remove non-error logging from this class.
    LOG(WARNING) << "Starting lacros component load.";
    cros_component_manager_->Load(kLacrosComponentName,
                                  CrOSComponentManager::MountPolicy::kMount,
                                  CrOSComponentManager::UpdatePolicy::kForce,
                                  base::BindOnce(&LacrosLoader::OnLoadComplete,
                                                 weak_factory_.GetWeakPtr()));
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&CheckIfPreviouslyInstalled, cros_component_manager_),
        base::BindOnce(&LacrosLoader::CleanUp, weak_factory_.GetWeakPtr()));
  }
}

bool LacrosLoader::IsReady() const {
  return !lacros_path_.empty();
}

void LacrosLoader::SetReadyCallback(base::OnceClosure ready_callback) {
  ready_callback_ = std::move(ready_callback);
}

void LacrosLoader::Start() {
  if (!IsLacrosAllowed())
    return;

  std::string chrome_path;
  if (chromeos::features::IsLacrosComponentUpdaterEnabled()) {
    if (lacros_path_.empty()) {
      LOG(WARNING) << "lacros component image not yet available";
      return;
    }
    chrome_path = lacros_path_.MaybeAsASCII() + "/chrome";
  } else {
    // This is the default path on eve-lacros images.
    chrome_path = "/usr/local/lacros/chrome";
  }
  LOG(WARNING) << "Launching lacros-chrome at " << chrome_path;

  base::LaunchOptions options;
  options.environment["EGL_PLATFORM"] = "surfaceless";
  options.environment["XDG_RUNTIME_DIR"] = "/run/chrome";
  options.kill_on_parent_death = true;

  std::vector<std::string> argv = {
      chrome_path,
      "--ozone-platform=wayland",
      std::string("--user-data-dir=") + kUserDataDir,
      "--enable-gpu-rasterization",
      "--enable-oop-rasterization",
      "--lang=en-US",
      "--breakpad-dump-location=/tmp"};
  lacros_process_ = base::LaunchProcess(argv, options);
  LOG(WARNING) << "Launched lacros-chrome with pid " << lacros_process_.Pid();
}

void LacrosLoader::OnLoadComplete(
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  if (error != CrOSComponentManager::Error::NONE) {
    LOG(WARNING) << "Error loading lacros component image: "
                 << static_cast<int>(error);
    return;
  }
  lacros_path_ = path;
  LOG(WARNING) << "Loaded lacros image at " << lacros_path_.MaybeAsASCII();
  if (ready_callback_)
    std::move(ready_callback_).Run();
}

void LacrosLoader::CleanUp(bool previously_installed) {
  if (previously_installed)
    cros_component_manager_->Unload(kLacrosComponentName);
}
