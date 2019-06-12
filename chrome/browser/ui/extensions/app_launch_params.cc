// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/app_launch_params.h"

#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/base/window_open_disposition.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "components/arc/arc_util.h"
#endif

using extensions::ExtensionPrefs;

AppLaunchParams::AppLaunchParams(Profile* profile,
                                 const web_app::AppId& app_id,
                                 extensions::LaunchContainer container,
                                 WindowOpenDisposition disposition,
                                 extensions::AppLaunchSource source,
                                 int64_t display_id)
    : profile(profile),
      app_id(app_id),
      container(container),
      disposition(disposition),
      command_line(base::CommandLine::NO_PROGRAM),
      source(source),
      display_id(display_id),
      opener(nullptr) {}

AppLaunchParams::AppLaunchParams(const AppLaunchParams& other) = default;

AppLaunchParams::~AppLaunchParams() {}

AppLaunchParams CreateAppLaunchParamsUserContainer(
    Profile* profile,
    const extensions::Extension* extension,
    WindowOpenDisposition disposition,
    extensions::AppLaunchSource source) {
  // Look up the app preference to find out the right launch container. Default
  // is to launch as a regular tab.
  extensions::LaunchContainer container =
      extensions::GetLaunchContainer(ExtensionPrefs::Get(profile), extension);
  return AppLaunchParams(profile, extension->id(), container, disposition,
                         source);
}

AppLaunchParams CreateAppLaunchParamsWithEventFlags(
    Profile* profile,
    const extensions::Extension* extension,
    int event_flags,
    extensions::AppLaunchSource source,
    int64_t display_id) {
  WindowOpenDisposition raw_disposition =
      ui::DispositionFromEventFlags(event_flags);

  extensions::LaunchContainer container;
  WindowOpenDisposition disposition;
  if (raw_disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
      raw_disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    container = extensions::LAUNCH_CONTAINER_TAB;
    disposition = raw_disposition;
  } else if (raw_disposition == WindowOpenDisposition::NEW_WINDOW) {
    container = extensions::LAUNCH_CONTAINER_WINDOW;
    disposition = raw_disposition;
  } else {
    // Look at preference to find the right launch container.  If no preference
    // is set, launch as a regular tab.
    container =
        extensions::GetLaunchContainer(ExtensionPrefs::Get(profile), extension);
    disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  }
  return AppLaunchParams(profile, extension->id(), container, disposition,
                         source, display_id);
}
