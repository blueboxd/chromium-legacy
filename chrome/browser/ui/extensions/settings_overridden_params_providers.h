// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_PARAMS_PROVIDERS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_PARAMS_PROVIDERS_H_

#include "base/optional.h"
#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"

namespace settings_overridden_params {

// Retrieves the params for displaying the NTP setting overridden dialog, if
// there is a controlling extension. Otherwise, returns an empty optional.
base::Optional<ExtensionSettingsOverriddenDialog::Params>
GetNtpOverriddenParams(Profile* profile);

}  // namespace settings_overridden_params

#endif  // CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_PARAMS_PROVIDERS_H_
