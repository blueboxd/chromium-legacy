// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LACROS_LACROS_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_LACROS_LACROS_LOADER_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/component_updater/cros_component_manager.h"

// Manages download and launch of the lacros-chrome binary.
class LacrosLoader {
 public:
  // Direct getter because there are no accessors to the owning object.
  static LacrosLoader* Get();

  explicit LacrosLoader(component_updater::CrOSComponentManager* manager);
  LacrosLoader(const LacrosLoader&) = delete;
  LacrosLoader& operator=(const LacrosLoader&) = delete;
  ~LacrosLoader();

  void Init();

  // Starts the lacros-chrome binary.
  void Start();

 private:
  void OnLoadComplete(component_updater::CrOSComponentManager::Error error,
                      const base::FilePath& path);

  component_updater::CrOSComponentManager* cros_component_manager_;

  // Path to the lacros-chrome disk image directory.
  base::FilePath lacros_path_;

  base::WeakPtrFactory<LacrosLoader> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CHROMEOS_LACROS_LACROS_LOADER_H_
