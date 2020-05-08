// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/crostini_section.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/chromeos/crostini_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetCrostiniSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI,
       mojom::kCrostiniDetailsSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCrostiniDetails},
       {IDS_OS_SETTINGS_TAG_CROSTINI_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCrostiniOptedInSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI_USB_PREFERENCES,
       mojom::kCrostiniUsbPreferencesSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCrostiniUsbPreferences},
       {IDS_OS_SETTINGS_TAG_CROSTINI_USB_PREFERENCES_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_CROSTINI_REMOVE,
       mojom::kCrostiniDetailsSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kUninstallCrostini},
       {
           IDS_OS_SETTINGS_TAG_CROSTINI_REMOVE_ALT1,
           IDS_OS_SETTINGS_TAG_CROSTINI_REMOVE_ALT2,
           IDS_OS_SETTINGS_TAG_CROSTINI_REMOVE_ALT3,
           IDS_OS_SETTINGS_TAG_CROSTINI_REMOVE_ALT4,
           IDS_OS_SETTINGS_TAG_CROSTINI_REMOVE_ALT5,
       }},
      {IDS_OS_SETTINGS_TAG_CROSTINI_SHARED_FOLDERS,
       mojom::kCrostiniManageSharedFoldersSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCrostiniManageSharedFolders},
       {IDS_OS_SETTINGS_TAG_CROSTINI_SHARED_FOLDERS_ALT1,
        IDS_OS_SETTINGS_TAG_CROSTINI_SHARED_FOLDERS_ALT2,
        IDS_OS_SETTINGS_TAG_CROSTINI_SHARED_FOLDERS_ALT3,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCrostiniOptedOutSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI_SETUP,
       mojom::kCrostiniSectionPath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kCrostini},
       {IDS_OS_SETTINGS_TAG_CROSTINI_SETUP_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCrostiniExportImportSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI_BACKUP_RESTORE,
       mojom::kCrostiniBackupAndRestoreSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCrostiniBackupAndRestore},
       {IDS_OS_SETTINGS_TAG_CROSTINI_BACKUP_RESTORE_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCrostiniAdbSideloadingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI_ANDROID_APPS_ADB,
       mojom::kCrostiniDevelopAndroidAppsSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCrostiniAdbDebugging},
       {IDS_OS_SETTINGS_TAG_CROSTINI_ANDROID_APPS_ADB_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_CROSTINI_ANDROID_APPS,
       mojom::kCrostiniDevelopAndroidAppsSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCrostiniDevelopAndroidApps},
       {IDS_OS_SETTINGS_TAG_CROSTINI_ANDROID_APPS_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCrostiniPortForwardingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI_PORT_FORWARDING,
       mojom::kCrostiniPortForwardingSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCrostiniPortForwarding},
       {IDS_OS_SETTINGS_TAG_CROSTINI_PORT_FORWARDING_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCrostiniContainerUpgradeSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI_CONTAINER_UPGRADE,
       mojom::kCrostiniDetailsSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCrostiniContainerUpgrade},
       {IDS_OS_SETTINGS_TAG_CROSTINI_CONTAINER_UPGRADE_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCrostiniDiskResizingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI_DISK_RESIZE,
       mojom::kCrostiniDetailsSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.setting = mojom::Setting::kCrostiniDiskResize},
       {IDS_OS_SETTINGS_TAG_CROSTINI_DISK_RESIZE_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCrostiniMicSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI_MIC_ACCESS,
       mojom::kCrostiniDetailsSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCrostiniMicAccess},
       {IDS_OS_SETTINGS_TAG_CROSTINI_MIC_ACCESS_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

bool IsProfileManaged(Profile* profile) {
  return profile->GetProfilePolicyConnector()->IsManaged();
}

bool IsDeviceManaged() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsEnterpriseManaged();
}

bool IsAdbSideloadingAllowed() {
  return base::FeatureList::IsEnabled(features::kArcAdbSideloadingFeature);
}

bool IsPortForwardingAllowed() {
  return base::FeatureList::IsEnabled(features::kCrostiniPortForwarding);
}

bool IsDiskResizingAllowed() {
  return base::FeatureList::IsEnabled(features::kCrostiniDiskResizing);
}

bool IsMicSettingAllowed() {
  return base::FeatureList::IsEnabled(features::kCrostiniShowMicSetting);
}

}  // namespace

CrostiniSection::CrostiniSection(Profile* profile,
                                 Delegate* per_page_delegate,
                                 PrefService* pref_service)
    : OsSettingsSection(profile, per_page_delegate),
      pref_service_(pref_service) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      crostini::prefs::kUserCrostiniAllowedByPolicy,
      base::BindRepeating(&CrostiniSection::UpdateSearchTags,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      crostini::prefs::kCrostiniEnabled,
      base::BindRepeating(&CrostiniSection::UpdateSearchTags,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy,
      base::BindRepeating(&CrostiniSection::UpdateSearchTags,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      arc::prefs::kArcEnabled,
      base::BindRepeating(&CrostiniSection::UpdateSearchTags,
                          base::Unretained(this)));
  UpdateSearchTags();
}

CrostiniSection::~CrostiniSection() = default;

void CrostiniSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"crostiniPageTitle", IDS_SETTINGS_CROSTINI_TITLE},
      {"crostiniPageLabel", IDS_SETTINGS_CROSTINI_LABEL},
      {"crostiniEnable", IDS_SETTINGS_TURN_ON},
      {"crostiniSharedPaths", IDS_SETTINGS_CROSTINI_SHARED_PATHS},
      {"crostiniSharedPathsListHeading",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_LIST_HEADING},
      {"crostiniSharedPathsInstructionsAdd",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_INSTRUCTIONS_ADD},
      {"crostiniSharedPathsInstructionsRemove",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_INSTRUCTIONS_REMOVE},
      {"crostiniSharedPathsRemoveSharing",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_REMOVE_SHARING},
      {"crostiniSharedPathsRemoveFailureDialogMessage",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_REMOVE_FAILURE_DIALOG_MESSAGE},
      {"crostiniSharedPathsRemoveFailureDialogTitle",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_REMOVE_FAILURE_DIALOG_TITLE},
      {"crostiniSharedPathsRemoveFailureTryAgain",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_REMOVE_FAILURE_TRY_AGAIN},
      {"crostiniSharedPathsListEmptyMessage",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_LIST_EMPTY_MESSAGE},
      {"crostiniExportImportTitle", IDS_SETTINGS_CROSTINI_EXPORT_IMPORT_TITLE},
      {"crostiniExport", IDS_SETTINGS_CROSTINI_EXPORT},
      {"crostiniExportLabel", IDS_SETTINGS_CROSTINI_EXPORT_LABEL},
      {"crostiniImport", IDS_SETTINGS_CROSTINI_IMPORT},
      {"crostiniImportLabel", IDS_SETTINGS_CROSTINI_IMPORT_LABEL},
      {"crostiniImportConfirmationDialogTitle",
       IDS_SETTINGS_CROSTINI_CONFIRM_IMPORT_DIALOG_WINDOW_TITLE},
      {"crostiniImportConfirmationDialogMessage",
       IDS_SETTINGS_CROSTINI_CONFIRM_IMPORT_DIALOG_WINDOW_MESSAGE},
      {"crostiniImportConfirmationDialogConfirmationButton",
       IDS_SETTINGS_CROSTINI_IMPORT},
      {"crostiniRemoveButton", IDS_SETTINGS_CROSTINI_REMOVE_BUTTON},
      {"crostiniSharedUsbDevicesLabel",
       IDS_SETTINGS_CROSTINI_SHARED_USB_DEVICES_LABEL},
      {"crostiniSharedUsbDevicesDescription",
       IDS_SETTINGS_CROSTINI_SHARED_USB_DEVICES_DESCRIPTION},
      {"crostiniSharedUsbDevicesExtraDescription",
       IDS_SETTINGS_CROSTINI_SHARED_USB_DEVICES_EXTRA_DESCRIPTION},
      {"crostiniSharedUsbDevicesListEmptyMessage",
       IDS_SETTINGS_CROSTINI_SHARED_USB_DEVICES_LIST_EMPTY_MESSAGE},
      {"crostiniArcAdbTitle", IDS_SETTINGS_CROSTINI_ARC_ADB_TITLE},
      {"crostiniArcAdbDescription", IDS_SETTINGS_CROSTINI_ARC_ADB_DESCRIPTION},
      {"crostiniArcAdbLabel", IDS_SETTINGS_CROSTINI_ARC_ADB_LABEL},
      {"crostiniArcAdbRestartButton",
       IDS_SETTINGS_CROSTINI_ARC_ADB_RESTART_BUTTON},
      {"crostiniArcAdbConfirmationTitleEnable",
       IDS_SETTINGS_CROSTINI_ARC_ADB_CONFIRMATION_TITLE_ENABLE},
      {"crostiniArcAdbConfirmationTitleDisable",
       IDS_SETTINGS_CROSTINI_ARC_ADB_CONFIRMATION_TITLE_DISABLE},
      {"crostiniContainerUpgrade",
       IDS_SETTINGS_CROSTINI_CONTAINER_UPGRADE_MESSAGE},
      {"crostiniContainerUpgradeSubtext",
       IDS_SETTINGS_CROSTINI_CONTAINER_UPGRADE_SUBTEXT},
      {"crostiniContainerUpgradeButton",
       IDS_SETTINGS_CROSTINI_CONTAINER_UPGRADE_BUTTON},
      {"crostiniPortForwarding", IDS_SETTINGS_CROSTINI_PORT_FORWARDING},
      {"crostiniPortForwardingDescription",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_DESCRIPTION},
      {"crostiniPortForwardingNoPorts",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_NO_PORTS},
      {"crostiniPortForwardingTableTitle",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_TABLE_TITLE},
      {"crostiniPortForwardingListPortNumber",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_LIST_PORT_NUMBER},
      {"crostiniPortForwardingListLabel",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_LIST_LABEL},
      {"crostiniPortForwardingAddPortButton",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_PORT_BUTTON},
      {"crostiniPortForwardingAddPortButtonDescription",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_PORT_BUTTON_DESCRIPTION},
      {"crostiniPortForwardingAddPortDialogTitle",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_PORT_DIALOG_TITLE},
      {"crostiniPortForwardingAddPortDialogLabel",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_PORT_DIALOG_LABEL},
      {"crostiniPortForwardingTCP", IDS_SETTINGS_CROSTINI_PORT_FORWARDING_TCP},
      {"crostiniPortForwardingUDP", IDS_SETTINGS_CROSTINI_PORT_FORWARDING_UDP},
      {"crostiniPortForwardingAddError",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_ERROR},
      {"crostiniPortForwardingRemoveAllPorts",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_REMOVE_ALL_PORTS},
      {"crostiniPortForwardingRemovePort",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_REMOVE_PORT},
      {"crostiniPortForwardingToggleAriaLabel",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_TOGGLE_PORT_ARIA_LABEL},
      {"crostiniPortForwardingRemoveAllPortsAriaLabel",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_REMOVE_ALL_PORTS_ARIA_LABEL},
      {"crostiniPortForwardingShowMoreActionsAriaLabel",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_SHOW_MORE_ACTIONS_ARIA_LABEL},
      {"crostiniDiskResizeTitle", IDS_SETTINGS_CROSTINI_DISK_RESIZE_TITLE},
      {"crostiniDiskResizeShowButton",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_SHOW_BUTTON},
      {"crostiniDiskResizeShowButtonAriaLabel",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_SHOW_BUTTON_ARIA_LABEL},
      {"crostiniDiskSizeCalculating", IDS_SETTINGS_STORAGE_SIZE_CALCULATING},
      {"crostiniDiskReserveSizeButton",
       IDS_SETTINGS_CROSTINI_DISK_RESERVE_SIZE_BUTTON},
      {"crostiniDiskReserveSizeButtonAriaLabel",
       IDS_SETTINGS_CROSTINI_DISK_RESERVE_SIZE_BUTTON_ARIA_LABEL},
      {"crostiniDiskResizeLabel", IDS_SETTINGS_CROSTINI_DISK_RESIZE_LABEL},
      {"crostiniDiskResizeDynamicallyAllocatedSubtext",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_DYNAMICALLY_ALLOCATED_SUBTEXT},
      {"crostiniDiskResizeUnsupported",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_UNSUPPORTED},
      {"crostiniDiskResizeLoading", IDS_SETTINGS_CROSTINI_DISK_RESIZE_LOADING},
      {"crostiniDiskResizeError", IDS_SETTINGS_CROSTINI_DISK_RESIZE_ERROR},
      {"crostiniDiskResizeErrorRetry",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_ERROR_RETRY},
      {"crostiniDiskResizeCancel", IDS_SETTINGS_CROSTINI_DISK_RESIZE_CANCEL},
      {"crostiniDiskResizeGoButton",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_GO_BUTTON},
      {"crostiniDiskResizeInProgress",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_IN_PROGRESS},
      {"crostiniDiskResizeResizingError",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_RESIZING_ERROR},
      {"crostiniDiskResizeConfirmationDialogTitle",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_CONFIRMATION_DIALOG_TITLE},
      {"crostiniDiskResizeConfirmationDialogMessage",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_CONFIRMATION_DIALOG_MESSAGE},
      {"crostiniDiskResizeConfirmationDialogButton",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_CONFIRMATION_DIALOG_BUTTON},
      {"crostiniDiskResizeDone", IDS_SETTINGS_CROSTINI_DISK_RESIZE_DONE},
      {"crostiniMicTitle", IDS_SETTINGS_CROSTINI_MIC_TITLE},
      {"crostiniMicDialogTitle", IDS_SETTINGS_CROSTINI_MIC_DIALOG_TITLE},
      {"crostiniMicDialogLabel", IDS_SETTINGS_CROSTINI_MIC_DIALOG_LABEL},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean("showCrostini", IsCrostiniAllowed());
  html_source->AddBoolean(
      "allowCrostini",
      crostini::CrostiniFeatures::Get()->IsUIAllowed(profile()));

  html_source->AddString(
      "crostiniSubtext",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_SUBTEXT, ui::GetChromeOSDeviceName(),
          GetHelpUrlWithBoard(chrome::kLinuxAppsLearnMoreURL)));
  html_source->AddString(
      "crostiniArcAdbPowerwashRequiredSublabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_ARC_ADB_POWERWASH_REQUIRED_SUBLABEL,
          base::ASCIIToUTF16(chrome::kArcAdbSideloadingLearnMoreURL)));
  html_source->AddString("crostiniRemove", l10n_util::GetStringFUTF16(
                                               IDS_SETTINGS_CROSTINI_REMOVE,
                                               ui::GetChromeOSDeviceName()));
  html_source->AddString(
      "crostiniArcAdbConfirmationMessageEnable",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_ARC_ADB_CONFIRMATION_MESSAGE_ENABLE,
          ui::GetChromeOSDeviceName()));
  html_source->AddString(
      "crostiniArcAdbConfirmationMessageDisable",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_ARC_ADB_CONFIRMATION_MESSAGE_DISABLE,
          ui::GetChromeOSDeviceName()));
  html_source->AddString(
      "crostiniSharedPathsInstructionsLocate",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_SHARED_PATHS_INSTRUCTIONS_LOCATE,
          base::ASCIIToUTF16(
              crostini::ContainerChromeOSBaseDirectory().value())));
  html_source->AddBoolean("showCrostiniExportImport", IsExportImportAllowed());
  html_source->AddBoolean("arcAdbSideloadingSupported",
                          IsAdbSideloadingAllowed());
  html_source->AddBoolean("showCrostiniPortForwarding",
                          IsPortForwardingAllowed());
  html_source->AddBoolean("isOwnerProfile",
                          chromeos::ProfileHelper::IsOwnerProfile(profile()));
  html_source->AddBoolean("isEnterpriseManaged",
                          IsDeviceManaged() || IsProfileManaged(profile()));
  html_source->AddBoolean(
      "canChangeAdbSideloading",
      crostini::CrostiniFeatures::Get()->CanChangeAdbSideloading(profile()));
  html_source->AddBoolean("showCrostiniContainerUpgrade",
                          IsContainerUpgradeAllowed());
  html_source->AddBoolean("showCrostiniDiskResize", IsDiskResizingAllowed());
  html_source->AddBoolean("showCrostiniMic", IsMicSettingAllowed());
}

void CrostiniSection::AddHandlers(content::WebUI* web_ui) {
  if (crostini::CrostiniFeatures::Get()->IsUIAllowed(profile(),
                                                     /*check_policy=*/false)) {
    web_ui->AddMessageHandler(std::make_unique<CrostiniHandler>(profile()));
  }
}

bool CrostiniSection::IsCrostiniAllowed() {
  return crostini::CrostiniFeatures::Get()->IsUIAllowed(profile(),
                                                        /*check_policy=*/false);
}

bool CrostiniSection::IsExportImportAllowed() {
  return crostini::CrostiniFeatures::Get()->IsExportImportUIAllowed(profile());
}

bool CrostiniSection::IsContainerUpgradeAllowed() {
  return crostini::ShouldAllowContainerUpgrade(profile());
}

void CrostiniSection::UpdateSearchTags() {
  delegate()->RemoveSearchTags(GetCrostiniSearchConcepts());
  delegate()->RemoveSearchTags(GetCrostiniOptedInSearchConcepts());
  delegate()->RemoveSearchTags(GetCrostiniOptedOutSearchConcepts());
  delegate()->RemoveSearchTags(GetCrostiniExportImportSearchConcepts());
  delegate()->RemoveSearchTags(GetCrostiniAdbSideloadingSearchConcepts());
  delegate()->RemoveSearchTags(GetCrostiniPortForwardingSearchConcepts());
  delegate()->RemoveSearchTags(GetCrostiniContainerUpgradeSearchConcepts());
  delegate()->RemoveSearchTags(GetCrostiniDiskResizingSearchConcepts());
  delegate()->RemoveSearchTags(GetCrostiniMicSearchConcepts());

  if (!IsCrostiniAllowed())
    return;

  delegate()->AddSearchTags(GetCrostiniSearchConcepts());

  if (!pref_service_->GetBoolean(crostini::prefs::kCrostiniEnabled)) {
    delegate()->AddSearchTags(GetCrostiniOptedOutSearchConcepts());
    return;
  }

  delegate()->AddSearchTags(GetCrostiniOptedInSearchConcepts());

  if (IsExportImportAllowed())
    delegate()->AddSearchTags(GetCrostiniExportImportSearchConcepts());

  if (IsAdbSideloadingAllowed() &&
      pref_service_->GetBoolean(arc::prefs::kArcEnabled)) {
    delegate()->AddSearchTags(GetCrostiniAdbSideloadingSearchConcepts());
  }

  if (IsPortForwardingAllowed())
    delegate()->AddSearchTags(GetCrostiniPortForwardingSearchConcepts());

  if (IsContainerUpgradeAllowed())
    delegate()->AddSearchTags(GetCrostiniContainerUpgradeSearchConcepts());

  if (IsDiskResizingAllowed())
    delegate()->AddSearchTags(GetCrostiniDiskResizingSearchConcepts());

  if (IsMicSettingAllowed())
    delegate()->AddSearchTags(GetCrostiniMicSearchConcepts());
}

}  // namespace settings
}  // namespace chromeos
