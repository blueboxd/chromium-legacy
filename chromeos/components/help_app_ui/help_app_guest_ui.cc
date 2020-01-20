// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/help_app_ui/help_app_guest_ui.h"

#include "base/system/sys_info.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_help_app_bundle_resources.h"
#include "chromeos/grit/chromeos_help_app_bundle_resources_map.h"
#include "chromeos/grit/chromeos_help_app_resources.h"
#include "chromeos/system/statistics_provider.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/resources/grit/webui_resources.h"

namespace chromeos {

// static
content::WebUIDataSource* CreateHelpAppGuestDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIHelpAppGuestHost);
  source->AddResourcePath("app.html", IDR_HELP_APP_APP_HTML);
  source->AddResourcePath("app_bin.js", IDR_HELP_APP_APP_BIN_JS);
  source->AddResourcePath("load_time_data.js", IDR_WEBUI_JS_LOAD_TIME_DATA);

  // Add all resources from chromeos_media_app_bundle.pak.
  for (size_t i = 0; i < kChromeosHelpAppBundleResourcesSize; i++) {
    source->AddResourcePath(kChromeosHelpAppBundleResources[i].name,
                            kChromeosHelpAppBundleResources[i].value);
  }

  // Add strings that can be pulled in.
  source->AddString("boardName", base::SysInfo::GetLsbReleaseBoard());
  source->AddString("chromeOSVersion", base::SysInfo::OperatingSystemVersion());
  std::string customization_id;
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  provider->GetMachineStatistic(chromeos::system::kCustomizationIdKey,
                                &customization_id);
  source->AddString("customizationId", customization_id);
  source->UseStringsJs();

  // TODO(crbug.com/1023700): Better solution before launch.
  source->DisableDenyXFrameOptions();
  return source;
}

}  // namespace chromeos
