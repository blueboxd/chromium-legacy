// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_TERMINAL_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_TERMINAL_H_

#include <vector>

#include "chrome/browser/ui/extensions/app_launch_params.h"

class GURL;
class Browser;
class Profile;

namespace crostini {

// Generate the URL for Crostini terminal application.
GURL GenerateVshInCroshUrl(Profile* profile,
                           const std::string& vm_name,
                           const std::string& container_name,
                           const std::vector<std::string>& terminal_args);

// Generate AppLaunchParams for the Crostini terminal application.
AppLaunchParams GenerateTerminalAppLaunchParams(Profile* profile);

// Create the crosh-in-a-window that displays a shell in an container on a VM.
Browser* CreateContainerTerminal(const AppLaunchParams& launch_params,
                                 const GURL& vsh_in_crosh_url);

// Shows the already created crosh-in-a-window that displays a shell in an
// already running container on a VM.
void ShowContainerTerminal(const AppLaunchParams& launch_params,
                           const GURL& vsh_in_crosh_url,
                           Browser* browser);

// Launches the crosh-in-a-window that displays a shell in an already running
// container on a VM and passes |terminal_args| as parameters to that shell
// which will cause them to be executed as program inside that shell.
void LaunchContainerTerminal(Profile* profile,
                             const std::string& vm_name,
                             const std::string& container_name,
                             const std::vector<std::string>& terminal_args);

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_TERMINAL_H_
