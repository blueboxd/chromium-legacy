// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lacros/lacros_loader.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/upstart/upstart_client.h"

using component_updater::CrOSComponentManager;

namespace {
LacrosLoader* g_instance = nullptr;
}  // namespace

// static
LacrosLoader* LacrosLoader::Get() {
  return g_instance;
}

LacrosLoader::LacrosLoader(CrOSComponentManager* manager)
    : cros_component_manager_(manager) {
  DCHECK(cros_component_manager_);

  DCHECK(!g_instance);
  g_instance = this;
}

LacrosLoader::~LacrosLoader() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void LacrosLoader::Init() {
  const char kLacrosComponentName[] = "lacros-fishfood";
  if (chromeos::features::IsLacrosComponentUpdaterEnabled()) {
    cros_component_manager_->Load(kLacrosComponentName,
                                  CrOSComponentManager::MountPolicy::kMount,
                                  CrOSComponentManager::UpdatePolicy::kForce,
                                  base::BindOnce(&LacrosLoader::OnLoadComplete,
                                                 weak_factory_.GetWeakPtr()));
  } else {
    // Clean up any old disk images, since the user turned off the flag.
    cros_component_manager_->Unload(kLacrosComponentName);
  }
}

void LacrosLoader::Start() {
  std::vector<std::string> upstart_env;
  if (chromeos::features::IsLacrosComponentUpdaterEnabled()) {
    if (lacros_path_.empty()) {
      LOG(WARNING) << "lacros component image not yet available";
      return;
    }
    // Construct an environment-variable-like string with the binary path.
    std::string path_env = "LACROS_PATH=";
    path_env += lacros_path_.MaybeAsASCII();
    path_env += "/chrome";
    upstart_env.push_back(path_env);
  }
  chromeos::UpstartClient::Get()->StartLacrosChrome(upstart_env);
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
}
