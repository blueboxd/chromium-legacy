// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"

namespace chromeos {
namespace settings {

// static
base::string16 OsSettingsSection::GetHelpUrlWithBoard(
    const std::string& original_url) {
  return base::ASCIIToUTF16(original_url +
                            "&b=" + base::SysInfo::GetLsbReleaseBoard());
}

OsSettingsSection::~OsSettingsSection() = default;

OsSettingsSection::OsSettingsSection(Profile* profile, Delegate* delegate)
    : profile_(profile), delegate_(delegate) {
  DCHECK(profile);
  DCHECK(delegate);
}

}  // namespace settings
}  // namespace chromeos
