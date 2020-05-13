// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LACROS_LACROS_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_LACROS_LACROS_LOADER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "chrome/browser/component_updater/cros_component_manager.h"

// Manages download and launch of the lacros-chrome binary.
class LacrosLoader {
 public:
  // Direct getter because there are no accessors to the owning object.
  static LacrosLoader* Get();

  explicit LacrosLoader(
      scoped_refptr<component_updater::CrOSComponentManager> manager);
  LacrosLoader(const LacrosLoader&) = delete;
  LacrosLoader& operator=(const LacrosLoader&) = delete;
  ~LacrosLoader();

  void Init();

  // Returns true if the binary is ready to launch. Typical usage is to check
  // IsReady(), then if it returns false, call SetLoadCompleteCallback() to be
  // notified when the download completes.
  bool IsReady() const;

  // Sets a callback to be called when the binary download completes. The
  // download may not be successful.
  using LoadCompleteCallback = base::OnceCallback<void(bool success)>;
  void SetLoadCompleteCallback(LoadCompleteCallback callback);

  // Starts the lacros-chrome binary.
  void Start();

 private:
  void OnLoadComplete(component_updater::CrOSComponentManager::Error error,
                      const base::FilePath& path);

  // Removes any state that Lacros left behind.
  void CleanUp(bool previously_installed);

  scoped_refptr<component_updater::CrOSComponentManager>
      cros_component_manager_;

  // Path to the lacros-chrome disk image directory.
  base::FilePath lacros_path_;

  // Called when the binary download completes.
  LoadCompleteCallback load_complete_callback_;

  // Process handle for the lacros-chrome process.
  base::Process lacros_process_;

  base::WeakPtrFactory<LacrosLoader> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CHROMEOS_LACROS_LACROS_LOADER_H_
