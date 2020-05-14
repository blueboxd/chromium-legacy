// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lacros/lacros_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/lacros/lacros_util.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "google_apis/google_api_keys.h"

using component_updater::CrOSComponentManager;

namespace {

LacrosLoader* g_instance = nullptr;

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
  if (!lacros_util::IsLacrosAllowed())
    return;

  if (chromeos::features::IsLacrosSupportEnabled()) {
    // TODO(crbug.com/1078607): Remove non-error logging from this class.
    LOG(WARNING) << "Starting lacros component load.";

    // If the user has specified a path for the lacros-chrome binary, use that
    // rather than component manager.
    base::FilePath lacros_chrome_path =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            chromeos::switches::kLacrosChromePath);
    if (!lacros_chrome_path.empty()) {
      OnLoadComplete(CrOSComponentManager::Error::NONE, lacros_chrome_path);
      return;
    }

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

void LacrosLoader::SetLoadCompleteCallback(LoadCompleteCallback callback) {
  load_complete_callback_ = std::move(callback);
}

void LacrosLoader::Start() {
  if (!lacros_util::IsLacrosAllowed())
    return;

  // TODO(jamescook): Provide a switch to override the lacros-chrome path for
  // developers.
  std::string chrome_path;
  if (lacros_path_.empty()) {
    LOG(WARNING) << "lacros component image not yet available";
    return;
  }
  chrome_path = lacros_path_.MaybeAsASCII() + "/chrome";
  LOG(WARNING) << "Launching lacros-chrome at " << chrome_path;

  base::LaunchOptions options;
  options.environment["EGL_PLATFORM"] = "surfaceless";
  options.environment["XDG_RUNTIME_DIR"] = "/run/chrome";

  std::string api_key;
  if (google_apis::HasAPIKeyConfigured())
    api_key = google_apis::GetAPIKey();
  else
    api_key = google_apis::GetNonStableAPIKey();
  options.environment["GOOGLE_API_KEY"] = api_key;
  options.environment["GOOGLE_DEFAULT_CLIENT_ID"] =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  options.environment["GOOGLE_DEFAULT_CLIENT_SECRET"] =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);

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
  bool success = (error == CrOSComponentManager::Error::NONE);
  if (success) {
    lacros_path_ = path;
    LOG(WARNING) << "Loaded lacros image at " << lacros_path_.MaybeAsASCII();
  } else {
    LOG(WARNING) << "Error loading lacros component image: "
                 << static_cast<int>(error);
  }
  if (load_complete_callback_)
    std::move(load_complete_callback_).Run(success);
}

void LacrosLoader::CleanUp(bool previously_installed) {
  if (previously_installed)
    cros_component_manager_->Unload(kLacrosComponentName);
}
