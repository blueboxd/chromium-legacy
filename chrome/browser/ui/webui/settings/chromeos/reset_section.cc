// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/reset_section.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetResetSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_RESET,
       mojom::kResetSectionPath,
       mojom::SearchResultIcon::kReset,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kReset}},
      {IDS_OS_SETTINGS_TAG_RESET_POWERWASH,
       mojom::kResetSectionPath,
       mojom::SearchResultIcon::kReset,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPowerwash},
       {IDS_OS_SETTINGS_TAG_RESET_POWERWASH_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

bool IsPowerwashAllowed() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return !connector->IsEnterpriseManaged() &&
         !user_manager::UserManager::Get()->IsLoggedInAsGuest() &&
         !user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser() &&
         !user_manager::UserManager::Get()->IsLoggedInAsChildUser();
}

}  // namespace

ResetSection::ResetSection(Profile* profile, Delegate* per_page_delegate)
    : OsSettingsSection(profile, per_page_delegate) {
  if (IsPowerwashAllowed())
    delegate()->AddSearchTags(GetResetSearchConcepts());
}

ResetSection::~ResetSection() = default;

void ResetSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"resetPageTitle", IDS_SETTINGS_RESET},
      {"powerwashTitle", IDS_SETTINGS_FACTORY_RESET},
      {"powerwashDialogTitle", IDS_SETTINGS_FACTORY_RESET_HEADING},
      {"powerwashDialogButton", IDS_SETTINGS_RESTART},
      {"powerwashButton", IDS_SETTINGS_FACTORY_RESET_BUTTON_LABEL},
      {"powerwashDialogExplanation", IDS_SETTINGS_FACTORY_RESET_WARNING},
      {"powerwashLearnMoreUrl", IDS_FACTORY_RESET_HELP_URL},
      {"powerwashButtonRoleDescription",
       IDS_SETTINGS_FACTORY_RESET_BUTTON_ROLE},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean("allowPowerwash", IsPowerwashAllowed());

  html_source->AddBoolean(
      "showResetProfileBanner",
      ::settings::ResetSettingsHandler::ShouldShowResetProfileBanner(
          profile()));

  html_source->AddString(
      "powerwashDescription",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_FACTORY_RESET_DESCRIPTION,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
}

void ResetSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<::settings::ResetSettingsHandler>(profile()));
}

}  // namespace settings
}  // namespace chromeos
