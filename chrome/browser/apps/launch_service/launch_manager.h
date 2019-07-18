// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_MANAGER_H_
#define CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_MANAGER_H_

#include <string>

#include "base/macros.h"

class Profile;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace apps {

// A LaunchManager handles launch requests for a given type of apps.
class LaunchManager {
 public:
  virtual ~LaunchManager();

  // Attempt to open |app_id| in a new window.
  virtual bool OpenApplicationWindow(
      const std::string& app_id,
      const base::CommandLine& command_line,
      const base::FilePath& current_directory) = 0;

  // Attempt to open |app_id| in a new tab.
  virtual bool OpenApplicationTab(const std::string& app_id) = 0;

 protected:
  explicit LaunchManager(Profile*);
  Profile* profile() { return profile_; }

 private:
  Profile* const profile_;
  DISALLOW_COPY_AND_ASSIGN(LaunchManager);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_MANAGER_H_
