// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_terminal.h"

#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/ash/window_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "net/base/escape.h"

namespace crostini {

GURL GenerateVshInCroshUrl(Profile* profile,
                           const std::string& vm_name,
                           const std::string& container_name,
                           const std::vector<std::string>& terminal_args) {
  std::string vsh_crosh = base::StringPrintf(
      "chrome-extension://%s/html/crosh.html?command=vmshell",
      kCrostiniCroshBuiltinAppId);
  std::string vm_name_param = net::EscapeQueryParamValue(
      base::StringPrintf("--vm_name=%s", vm_name.c_str()), false);
  std::string container_name_param = net::EscapeQueryParamValue(
      base::StringPrintf("--target_container=%s", container_name.c_str()),
      false);
  std::string owner_id_param = net::EscapeQueryParamValue(
      base::StringPrintf("--owner_id=%s",
                         CryptohomeIdForProfile(profile).c_str()),
      false);

  std::vector<std::string> pieces = {vsh_crosh, vm_name_param,
                                     container_name_param, owner_id_param};
  if (!terminal_args.empty()) {
    // Separates the command args from the args we are passing into the
    // terminal to be executed.
    pieces.push_back("--");
    for (auto arg : terminal_args) {
      pieces.push_back(net::EscapeQueryParamValue(arg, false));
    }
  }

  return GURL(base::JoinString(pieces, "&args[]="));
}

AppLaunchParams GenerateTerminalAppLaunchParams(Profile* profile) {
  AppLaunchParams launch_params(
      profile, kCrostiniCroshBuiltinAppId,
      apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW,
      apps::mojom::AppLaunchSource::kSourceAppLauncher);
  launch_params.override_app_name =
      AppNameFromCrostiniAppId(kCrostiniTerminalId);
  return launch_params;
}

Browser* CreateContainerTerminal(const AppLaunchParams& launch_params,
                                 const GURL& vsh_in_crosh_url) {
  return CreateApplicationWindow(launch_params, vsh_in_crosh_url);
}

void ShowContainerTerminal(const AppLaunchParams& launch_params,
                           const GURL& vsh_in_crosh_url,
                           Browser* browser) {
  ShowApplicationWindow(launch_params, vsh_in_crosh_url, browser,
                        WindowOpenDisposition::NEW_FOREGROUND_TAB);
  browser->window()->GetNativeWindow()->SetProperty(
      kOverrideWindowIconResourceIdKey, IDR_LOGO_CROSTINI_TERMINAL);
}

void LaunchContainerTerminal(Profile* profile,
                             const std::string& vm_name,
                             const std::string& container_name,
                             const std::vector<std::string>& terminal_args) {
  GURL vsh_in_crosh_url =
      GenerateVshInCroshUrl(profile, vm_name, container_name, terminal_args);
  AppLaunchParams launch_params = GenerateTerminalAppLaunchParams(profile);

  Browser* browser = CreateContainerTerminal(launch_params, vsh_in_crosh_url);
  ShowContainerTerminal(launch_params, vsh_in_crosh_url, browser);
}

}  // namespace crostini
