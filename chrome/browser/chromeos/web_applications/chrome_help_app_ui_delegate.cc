// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/chrome_help_app_ui_delegate.h"

#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/mojom/assistant_state_controller.mojom.h"
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/assistant/assistant_util.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/system/statistics_provider.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/events/devices/device_data_manager.h"
#include "url/gurl.h"

ChromeHelpAppUIDelegate::ChromeHelpAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

base::Optional<std::string> ChromeHelpAppUIDelegate::OpenFeedbackDialog() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  // TODO(crbug/1045222): Additional strings are blank right now while we decide
  // on the language and relevant information we want feedback to include.
  // Note that category_tag is the name of the listnr bucket we want our
  // reports to end up in. I.e DESKTOP_TAB_GROUPS.
  chrome::ShowFeedbackPage(
      GURL(chromeos::kChromeUIHelpAppURL), profile,
      chrome::kFeedbackSourceHelpApp, std::string() /* description_template */,
      std::string() /* description_placeholder_text */,
      std::string() /* category_tag */, std::string() /* extra_diagnostics */);
  return base::nullopt;
}

void ChromeHelpAppUIDelegate::PopulateLoadTimeData(
    content::WebUIDataSource* source) {
  // Add strings that can be pulled in.
  source->AddString("boardName", base::SysInfo::GetLsbReleaseBoard());
  source->AddString("chromeOSVersion", base::SysInfo::OperatingSystemVersion());
  std::string customization_id;
  std::string hwid;
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  // MachineStatistics may not exist for browser tests, but it is fine for these
  // to be empty strings.
  provider->GetMachineStatistic(chromeos::system::kCustomizationIdKey,
                                &customization_id);
  provider->GetMachineStatistic(chromeos::system::kHardwareClassKey, &hwid);
  source->AddString("customizationId", customization_id);
  source->AddString("hwid", hwid);

  Profile* profile = Profile::FromWebUI(web_ui_);
  PrefService* pref_service = profile->GetPrefs();

  // Checks if any of the MultiDevice features (e.g. Instant Tethering,
  // Messages, Smart Lock) is allowed on this device.
  source->AddBoolean(
      "multiDeviceFeaturesAllowed",
      chromeos::multidevice_setup::AreAnyMultiDeviceFeaturesAllowed(
          pref_service));
  source->AddBoolean("tabletMode", ash::TabletMode::Get()->InTabletMode());
  // Checks if there are active touch screens.
  source->AddBoolean(
      "hasTouchScreen",
      ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices().empty());
  // Checks if the Google Assistant is allowed on this device by going through
  // policies.
  ash::mojom::AssistantAllowedState assistant_allowed_state =
      assistant::IsAssistantAllowedForProfile(profile);
  source->AddBoolean(
      "assistantAllowed",
      assistant_allowed_state == ash::mojom::AssistantAllowedState::ALLOWED);
  source->AddBoolean(
      "assistantEnabled",
      ash::AssistantState::Get()->settings_enabled().value_or(false));
  source->AddBoolean("playStoreEnabled",
                     arc::IsArcPlayStoreEnabledForProfile(profile));
  source->AddBoolean("pinEnabled",
                     chromeos::quick_unlock::IsPinEnabled(pref_service));
}
