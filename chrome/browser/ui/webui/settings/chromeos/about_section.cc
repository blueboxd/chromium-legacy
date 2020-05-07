// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/about_section.h"

#include "base/i18n/message_formatter.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "chrome/browser/ui/webui/management_ui.h"
#include "chrome/browser/ui/webui/settings/about_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "components/version_ui/version_ui_constants.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetAboutSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ABOUT_CHROME_OS_DETAILED_BUILD,
       mojom::kDetailedBuildInfoSubpagePath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kDetailedBuildInfo},
       {IDS_OS_SETTINGS_TAG_ABOUT_CHROME_OS_DETAILED_BUILD_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_ABOUT_CHROME_OS,
       mojom::kAboutChromeOsDetailsSubpagePath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kAboutChromeOsDetails}},
      {IDS_OS_SETTINGS_TAG_ABOUT_CHROME_OS_VERSION,
       mojom::kAboutChromeOsDetailsSubpagePath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kAboutChromeOsDetails}},
      {IDS_OS_SETTINGS_TAG_ABOUT_CHROME_OS_CHANNEL,
       mojom::kDetailedBuildInfoSubpagePath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kChangeChromeChannel}},
  });
  return *tags;
}

// Returns the link to the safety info for the device (if it exists).
std::string GetSafetyInfoLink() {
  const std::vector<std::string> board =
      base::SplitString(base::SysInfo::GetLsbReleaseBoard(), "-",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (board[0] == "nocturne") {
    return chrome::kChromeUISafetyPixelSlateURL;
  }
  if (board[0] == "eve" || board[0] == "atlas") {
    return chrome::kChromeUISafetyPixelbookURL;
  }

  return std::string();
}

// Returns true if the device is enterprise managed, false otherwise.
bool IsEnterpriseManaged() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsEnterpriseManaged();
}

bool IsDeviceManaged() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsEnterpriseManaged();
}

}  // namespace

AboutSection::AboutSection(Profile* profile, Delegate* per_page_delegate)
    : OsSettingsSection(profile, per_page_delegate) {
  delegate()->AddSearchTags(GetAboutSearchConcepts());
}

AboutSection::~AboutSection() = default;

void AboutSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  // Top level About page strings.
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"aboutProductLogoAlt", IDS_SHORT_PRODUCT_LOGO_ALT_TEXT},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"aboutReportAnIssue", IDS_SETTINGS_ABOUT_PAGE_REPORT_AN_ISSUE},
#endif
    {"aboutRelaunch", IDS_SETTINGS_ABOUT_PAGE_RELAUNCH},
    {"aboutUpgradeCheckStarted", IDS_SETTINGS_ABOUT_UPGRADE_CHECK_STARTED},
    {"aboutUpgradeRelaunch", IDS_SETTINGS_UPGRADE_SUCCESSFUL_RELAUNCH},
    {"aboutUpgradeUpdating", IDS_SETTINGS_UPGRADE_UPDATING},
    {"aboutUpgradeUpdatingPercent", IDS_SETTINGS_UPGRADE_UPDATING_PERCENT},
    {"aboutGetHelpUsingChrome", IDS_SETTINGS_GET_HELP_USING_CHROME},
    {"aboutPageTitle", IDS_SETTINGS_ABOUT_PROGRAM},
    {"aboutProductTitle", IDS_PRODUCT_NAME},

    {"aboutEndOfLifeTitle", IDS_SETTINGS_ABOUT_PAGE_END_OF_LIFE_TITLE},
    {"aboutRelaunchAndPowerwash",
     IDS_SETTINGS_ABOUT_PAGE_RELAUNCH_AND_POWERWASH},
    {"aboutRollbackInProgress", IDS_SETTINGS_UPGRADE_ROLLBACK_IN_PROGRESS},
    {"aboutRollbackSuccess", IDS_SETTINGS_UPGRADE_ROLLBACK_SUCCESS},
    {"aboutUpgradeUpdatingChannelSwitch",
     IDS_SETTINGS_UPGRADE_UPDATING_CHANNEL_SWITCH},
    {"aboutUpgradeSuccessChannelSwitch",
     IDS_SETTINGS_UPGRADE_SUCCESSFUL_CHANNEL_SWITCH},
    {"aboutTPMFirmwareUpdateTitle",
     IDS_SETTINGS_ABOUT_TPM_FIRMWARE_UPDATE_TITLE},
    {"aboutTPMFirmwareUpdateDescription",
     IDS_SETTINGS_ABOUT_TPM_FIRMWARE_UPDATE_DESCRIPTION},

    // About page, channel switcher dialog.
    {"aboutChangeChannel", IDS_SETTINGS_ABOUT_PAGE_CHANGE_CHANNEL},
    {"aboutChangeChannelAndPowerwash",
     IDS_SETTINGS_ABOUT_PAGE_CHANGE_CHANNEL_AND_POWERWASH},
    {"aboutDelayedWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_DELAYED_WARNING_MESSAGE},
    {"aboutDelayedWarningTitle", IDS_SETTINGS_ABOUT_PAGE_DELAYED_WARNING_TITLE},
    {"aboutPowerwashWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_POWERWASH_WARNING_MESSAGE},
    {"aboutPowerwashWarningTitle",
     IDS_SETTINGS_ABOUT_PAGE_POWERWASH_WARNING_TITLE},
    {"aboutUnstableWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_UNSTABLE_WARNING_MESSAGE},
    {"aboutUnstableWarningTitle",
     IDS_SETTINGS_ABOUT_PAGE_UNSTABLE_WARNING_TITLE},
    {"aboutChannelDialogBeta", IDS_SETTINGS_ABOUT_PAGE_DIALOG_CHANNEL_BETA},
    {"aboutChannelDialogDev", IDS_SETTINGS_ABOUT_PAGE_DIALOG_CHANNEL_DEV},
    {"aboutChannelDialogStable", IDS_SETTINGS_ABOUT_PAGE_DIALOG_CHANNEL_STABLE},

    // About page, update warning dialog.
    {"aboutUpdateWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_UPDATE_WARNING_MESSAGE},
    {"aboutUpdateWarningTitle", IDS_SETTINGS_ABOUT_PAGE_UPDATE_WARNING_TITLE},

    // Detailed build information
    {"aboutBuildDetailsTitle", IDS_OS_SETTINGS_ABOUT_PAGE_BUILD_DETAILS},
    {"aboutChannelBeta", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_BETA},
    {"aboutChannelCanary", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_CANARY},
    {"aboutChannelDev", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_DEV},
    {"aboutChannelLabel", IDS_SETTINGS_ABOUT_PAGE_CHANNEL},
    {"aboutChannelStable", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_STABLE},
    {"aboutCheckForUpdates", IDS_SETTINGS_ABOUT_PAGE_CHECK_FOR_UPDATES},
    {"aboutCurrentlyOnChannel", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL},
    {"aboutDetailedBuildInfo", IDS_SETTINGS_ABOUT_PAGE_DETAILED_BUILD_INFO},
    {version_ui::kApplicationLabel, IDS_PRODUCT_NAME},
    {version_ui::kPlatform, IDS_PLATFORM_LABEL},
    {version_ui::kFirmwareVersion, IDS_VERSION_UI_FIRMWARE_VERSION},
    {version_ui::kARC, IDS_ARC_LABEL},
    {"aboutBuildDetailsCopyTooltipLabel",
     IDS_OS_SETTINGS_ABOUT_PAGE_BUILD_DETAILS_COPY_TOOLTIP_LABEL},
    {"aboutIsArcStatusTitle", IDS_OS_SETTINGS_ABOUT_ARC_STATUS_TITLE},
    {"aboutIsDeveloperModeTitle", IDS_OS_SETTINGS_ABOUT_DEVELOPER_MODE},
    {"isEnterpriseManagedTitle",
     IDS_OS_SETTINGS_ABOUT_PAGE_ENTERPRISE_ENNROLLED_TITLE},
    {"aboutOsPageTitle", IDS_SETTINGS_ABOUT_OS},
    {"aboutGetHelpUsingChromeOs", IDS_SETTINGS_GET_HELP_USING_CHROME_OS},
    {"aboutOsProductTitle", IDS_PRODUCT_OS_NAME},
    {"aboutReleaseNotesOffline", IDS_SETTINGS_ABOUT_PAGE_RELEASE_NOTES},
    {"aboutShowReleaseNotes", IDS_SETTINGS_ABOUT_PAGE_SHOW_RELEASE_NOTES},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString("aboutTPMFirmwareUpdateLearnMoreURL",
                         chrome::kTPMFirmwareUpdateLearnMoreURL);
  html_source->AddString(
      "aboutUpgradeUpToDate",
      ui::SubstituteChromeOSDeviceType(IDS_SETTINGS_UPGRADE_UP_TO_DATE));
  html_source->AddString("managementPage",
                         ManagementUI::GetManagementPageSubtitle(profile()));

  if (user_manager::UserManager::IsInitialized()) {
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    if (!IsDeviceManaged() && !user_manager->IsCurrentUserOwner()) {
      html_source->AddString("ownerEmail",
                             user_manager->GetOwnerAccountId().GetUserEmail());
    }
  }

  html_source->AddString(
      "aboutBrowserVersion",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ABOUT_PAGE_BROWSER_VERSION,
          base::UTF8ToUTF16(version_info::GetVersionNumber()),
          l10n_util::GetStringUTF16(version_info::IsOfficialBuild()
                                        ? IDS_VERSION_UI_OFFICIAL
                                        : IDS_VERSION_UI_UNOFFICIAL),
          base::UTF8ToUTF16(chrome::GetChannelName()),
          l10n_util::GetStringUTF16(sizeof(void*) == 8
                                        ? IDS_VERSION_UI_64BIT
                                        : IDS_VERSION_UI_32BIT)));
  html_source->AddString(
      "aboutProductCopyright",
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_COPYRIGHT),
          base::Time::Now()));

  base::string16 license = l10n_util::GetStringFUTF16(
      IDS_VERSION_UI_LICENSE, base::ASCIIToUTF16(chrome::kChromiumProjectURL),
      base::ASCIIToUTF16(chrome::kChromeUICreditsURL));
  html_source->AddString("aboutProductLicense", license);

  base::string16 os_license = l10n_util::GetStringFUTF16(
      IDS_ABOUT_CROS_VERSION_LICENSE,
      base::ASCIIToUTF16(chrome::kChromeUIOSCreditsURL));
  html_source->AddString("aboutProductOsLicense", os_license);
  base::string16 os_with_linux_license = l10n_util::GetStringFUTF16(
      IDS_ABOUT_CROS_WITH_LINUX_VERSION_LICENSE,
      base::ASCIIToUTF16(chrome::kChromeUIOSCreditsURL),
      base::ASCIIToUTF16(chrome::kChromeUICrostiniCreditsURL));
  html_source->AddString("aboutProductOsWithLinuxLicense",
                         os_with_linux_license);
  html_source->AddBoolean("aboutEnterpriseManaged", IsEnterpriseManaged());
  html_source->AddBoolean("aboutIsArcEnabled",
                          arc::IsArcPlayStoreEnabledForProfile(profile()));
  html_source->AddBoolean("aboutIsDeveloperMode",
                          base::CommandLine::ForCurrentProcess()->HasSwitch(
                              chromeos::switches::kSystemDevMode));

  html_source->AddString("endOfLifeMessage",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_ABOUT_PAGE_LAST_UPDATE_MESSAGE,
                             ui::GetChromeOSDeviceName(),
                             base::ASCIIToUTF16(chrome::kEolNotificationURL)));

  std::string safetyInfoLink = GetSafetyInfoLink();
  html_source->AddBoolean("shouldShowSafetyInfo", !safetyInfoLink.empty());

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  html_source->AddString("aboutTermsURL", chrome::kChromeUITermsURL);
  html_source->AddLocalizedString("aboutProductTos",
                                  IDS_ABOUT_TERMS_OF_SERVICE);
  html_source->AddString(
      "aboutProductSafety",
      l10n_util::GetStringUTF16(IDS_ABOUT_SAFETY_INFORMATION));
  html_source->AddString("aboutProductSafetyURL",
                         base::UTF8ToUTF16(safetyInfoLink));
#endif
}

void AboutSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(std::make_unique<::settings::AboutHandler>());
}

}  // namespace settings
}  // namespace chromeos
