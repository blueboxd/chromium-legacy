// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_extension_registration.h"

#include "chrome/browser/profiles/profile.h"

namespace web_app {

void RegisterFileHandlersForWebApp(
    const AppId& app_id,
    const std::string& app_name,
    const Profile& profile,
    const std::set<std::string>& file_extensions) {
  // TODO(davidbienvenu): Setup shim app and windows registry for this |app_id|.
}

void UnregisterFileHandlersForWebApp(const AppId& app_id,
                                     const Profile& profile) {
  // TODO(davidbienvenu): Cleanup windows registry entries for this
  // |app_id|.
}

}  // namespace web_app
