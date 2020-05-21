// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/apps_section.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/android_apps_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/plugin_vm_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/os_settings_resources.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetAppsSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_APPS,
       mojom::kAppsSectionPath,
       mojom::SearchResultIcon::kAppsGrid,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kApps}},
      {IDS_OS_SETTINGS_TAG_APPS_MANAGEMENT,
       mojom::kAppManagementSubpagePath,
       mojom::SearchResultIcon::kAppsGrid,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kAppManagement},
       {IDS_OS_SETTINGS_TAG_APPS_MANAGEMENT_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAndroidPlayStoreSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PLAY_STORE,
       mojom::kGooglePlayStoreSubpagePath,
       mojom::SearchResultIcon::kGooglePlay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kGooglePlayStore}},
      {IDS_OS_SETTINGS_TAG_REMOVE_PLAY_STORE,
       mojom::kGooglePlayStoreSubpagePath,
       mojom::SearchResultIcon::kGooglePlay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kRemovePlayStore},
       {IDS_OS_SETTINGS_TAG_REMOVE_PLAY_STORE_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAndroidSettingsSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ANDROID_SETTINGS_WITH_PLAY_STORE,
       mojom::kGooglePlayStoreSubpagePath,
       mojom::SearchResultIcon::kGooglePlay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kManageAndroidPreferences}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAndroidNoPlayStoreSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ANDROID_SETTINGS,
       mojom::kAppsSectionPath,
       mojom::SearchResultIcon::kAndroid,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kManageAndroidPreferences}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAndroidPlayStoreDisabledSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ANDROID_TURN_ON_PLAY_STORE,
       mojom::kAppsSectionPath,
       mojom::SearchResultIcon::kAndroid,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTurnOnPlayStore},
       {IDS_OS_SETTINGS_TAG_ANDROID_TURN_ON_PLAY_STORE_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

void AddAppManagementStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"appManagementAppInstalledByPolicyLabel",
       IDS_APP_MANAGEMENT_POLICY_APP_POLICY_STRING},
      {"appManagementCameraPermissionLabel", IDS_APP_MANAGEMENT_CAMERA},
      {"appManagementContactsPermissionLabel", IDS_APP_MANAGEMENT_CONTACTS},
      {"appManagementLocationPermissionLabel", IDS_APP_MANAGEMENT_LOCATION},
      {"appManagementMicrophonePermissionLabel", IDS_APP_MANAGEMENT_MICROPHONE},
      {"appManagementMoreSettingsLabel", IDS_APP_MANAGEMENT_MORE_SETTINGS},
      {"appManagementNoAppsFound", IDS_APP_MANAGEMENT_NO_APPS_FOUND},
      {"appManagementNoPermissions",
       IDS_APPLICATION_INFO_APP_NO_PERMISSIONS_TEXT},
      {"appManagementNotificationsLabel", IDS_APP_MANAGEMENT_NOTIFICATIONS},
      {"appManagementPermissionsLabel", IDS_APP_MANAGEMENT_PERMISSIONS},
      {"appManagementPinToShelfLabel", IDS_APP_MANAGEMENT_PIN_TO_SHELF},
      {"appManagementPrintingPermissionLabel", IDS_APP_MANAGEMENT_PRINTING},
      {"appManagementSearchPrompt", IDS_APP_MANAGEMENT_SEARCH_PROMPT},
      {"appManagementStoragePermissionLabel", IDS_APP_MANAGEMENT_STORAGE},
      {"appManagementUninstallLabel", IDS_APP_MANAGEMENT_UNINSTALL_APP},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
}

bool ShowPluginVm(const Profile* profile, const PrefService& pref_service) {
  // Even if not allowed, we still want to show Plugin VM if the VM image is on
  // disk, so that users are still able to delete the image at will.
  return plugin_vm::IsPluginVmAllowedForProfile(profile) ||
         pref_service.GetBoolean(plugin_vm::prefs::kPluginVmImageExists);
}

}  // namespace

AppsSection::AppsSection(Profile* profile,
                         SearchTagRegistry* search_tag_registry,
                         PrefService* pref_service,
                         ArcAppListPrefs* arc_app_list_prefs)
    : OsSettingsSection(profile, search_tag_registry),
      pref_service_(pref_service),
      arc_app_list_prefs_(arc_app_list_prefs) {
  registry()->AddSearchTags(GetAppsSearchConcepts());

  if (arc::IsArcAllowedForProfile(profile)) {
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        arc::prefs::kArcEnabled,
        base::BindRepeating(&AppsSection::UpdateAndroidSearchTags,
                            base::Unretained(this)));

    if (arc_app_list_prefs_)
      arc_app_list_prefs_->AddObserver(this);

    UpdateAndroidSearchTags();
  }
}

AppsSection::~AppsSection() {
  if (arc::IsArcAllowedForProfile(profile())) {
    if (arc_app_list_prefs_)
      arc_app_list_prefs_->RemoveObserver(this);
  }
}

void AppsSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"appsPageTitle", IDS_SETTINGS_APPS_TITLE},
      {"appManagementTitle", IDS_SETTINGS_APPS_LINK_TEXT},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddResourcePath("app-management/app_management.mojom-lite.js",
                               IDR_OS_SETTINGS_APP_MANAGEMENT_MOJO_LITE_JS);
  html_source->AddResourcePath(
      "app-management/types.mojom-lite.js",
      IDR_OS_SETTINGS_APP_MANAGEMENT_TYPES_MOJO_LITE_JS);
  html_source->AddResourcePath(
      "app-management/bitmap.mojom-lite.js",
      IDR_OS_SETTINGS_APP_MANAGEMENT_BITMAP_MOJO_LITE_JS);
  html_source->AddResourcePath(
      "app-management/file_path.mojom-lite.js",
      IDR_OS_SETTINGS_APP_MANAGEMENT_FILE_PATH_MOJO_LITE_JS);
  html_source->AddResourcePath(
      "app-management/image.mojom-lite.js",
      IDR_OS_SETTINGS_APP_MANAGEMENT_IMAGE_MOJO_LITE_JS);
  html_source->AddResourcePath(
      "app-management/image_info.mojom-lite.js",
      IDR_OS_SETTINGS_APP_MANAGEMENT_IMAGE_INFO_MOJO_LITE_JS);

  // We have 2 variants of Android apps settings. Default case, when the Play
  // Store app exists we show expandable section that allows as to
  // enable/disable the Play Store and link to Android settings which is
  // available once settings app is registered in the system.
  // For AOSP images we don't have the Play Store app. In last case we Android
  // apps settings consists only from root link to Android settings and only
  // visible once settings app is registered.
  html_source->AddBoolean("androidAppsVisible",
                          arc::IsArcAllowedForProfile(profile()));
  html_source->AddBoolean("havePlayStoreApp", arc::IsPlayStoreAvailable());
  html_source->AddBoolean(
      "isSupportedArcVersion",
      AppManagementPageHandler::IsCurrentArcVersionSupported(profile()));

  AddAppManagementStrings(html_source);
  AddAndroidAppStrings(html_source);
  AddPluginVmLoadTimeData(html_source);
}

void AppsSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::AndroidAppsHandler>(profile()));

  if (ShowPluginVm(profile(), *pref_service_)) {
    web_ui->AddMessageHandler(
        std::make_unique<chromeos::settings::PluginVmHandler>(profile()));
  }
}

int AppsSection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_APPS_TITLE;
}

mojom::Section AppsSection::GetSection() const {
  return mojom::Section::kApps;
}

mojom::SearchResultIcon AppsSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kAppsGrid;
}

std::string AppsSection::GetSectionPath() const {
  return mojom::kAppsSectionPath;
}

void AppsSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  // Manage apps.
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_APPS_LINK_TEXT,
                                     mojom::Subpage::kAppManagement,
                                     mojom::SearchResultIcon::kAppsGrid,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kAppManagementSubpagePath);
  // Note: The subpage name in the UI is updated dynamically based on the app
  // being shown, but we use a generic "App details" string here.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_APP_DETAILS_TITLE, mojom::Subpage::kAppDetails,
      mojom::Subpage::kAppManagement, mojom::SearchResultIcon::kAppsGrid,
      mojom::SearchResultDefaultRank::kMedium, mojom::kAppDetailsSubpagePath);
  generator->RegisterNestedSubpage(IDS_SETTINGS_APPS_PLUGIN_VM_SHARED_PATHS,
                                   mojom::Subpage::kPluginVmSharedPaths,
                                   mojom::Subpage::kAppManagement,
                                   mojom::SearchResultIcon::kAppsGrid,
                                   mojom::SearchResultDefaultRank::kMedium,
                                   mojom::kPluginVmSharedPathsSubpagePath);

  // Google Play Store.
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_ANDROID_APPS_LABEL,
                                     mojom::Subpage::kGooglePlayStore,
                                     mojom::SearchResultIcon::kGooglePlay,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kGooglePlayStoreSubpagePath);
  static constexpr mojom::Setting kGooglePlayStoreSettings[] = {
      mojom::Setting::kManageAndroidPreferences,
      mojom::Setting::kRemovePlayStore,
      mojom::Setting::kTurnOnPlayStore,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kGooglePlayStore,
                            kGooglePlayStoreSettings, generator);
}

void AppsSection::OnAppRegistered(const std::string& app_id,
                                  const ArcAppListPrefs::AppInfo& app_info) {
  UpdateAndroidSearchTags();
}

void AppsSection::AddAndroidAppStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"androidAppsPageLabel", IDS_SETTINGS_ANDROID_APPS_LABEL},
      {"androidAppsEnable", IDS_SETTINGS_TURN_ON},
      {"androidAppsManageApps", IDS_SETTINGS_ANDROID_APPS_MANAGE_APPS},
      {"androidAppsRemove", IDS_SETTINGS_ANDROID_APPS_REMOVE},
      {"androidAppsRemoveButton", IDS_SETTINGS_ANDROID_APPS_REMOVE_BUTTON},
      {"androidAppsDisableDialogTitle",
       IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_TITLE},
      {"androidAppsDisableDialogMessage",
       IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_MESSAGE},
      {"androidAppsDisableDialogRemove",
       IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_REMOVE},
      {"androidAppsManageAppLinks", IDS_SETTINGS_ANDROID_APPS_MANAGE_APP_LINKS},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
  html_source->AddLocalizedString("androidAppsPageTitle",
                                  arc::IsPlayStoreAvailable()
                                      ? IDS_SETTINGS_ANDROID_APPS_TITLE
                                      : IDS_SETTINGS_ANDROID_SETTINGS_TITLE);
  html_source->AddString(
      "androidAppsSubtext",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ANDROID_APPS_SUBTEXT, ui::GetChromeOSDeviceName(),
          GetHelpUrlWithBoard(chrome::kAndroidAppsLearnMoreURL)));
}

void AppsSection::AddPluginVmLoadTimeData(
    content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"pluginVmSharedPaths", IDS_SETTINGS_APPS_PLUGIN_VM_SHARED_PATHS},
      {"pluginVmSharedPathsListHeading",
       IDS_SETTINGS_APPS_PLUGIN_VM_SHARED_PATHS_LIST_HEADING},
      {"pluginVmSharedPathsInstructionsAdd",
       IDS_SETTINGS_APPS_PLUGIN_VM_SHARED_PATHS_INSTRUCTIONS_ADD},
      {"pluginVmSharedPathsInstructionsRemove",
       IDS_SETTINGS_APPS_PLUGIN_VM_SHARED_PATHS_INSTRUCTIONS_REMOVE},
      {"pluginVmSharedPathsRemoveSharing",
       IDS_SETTINGS_APPS_PLUGIN_VM_SHARED_PATHS_REMOVE_SHARING},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean("showPluginVm",
                          ShowPluginVm(profile(), *pref_service_));
}

void AppsSection::UpdateAndroidSearchTags() {
  registry()->RemoveSearchTags(GetAndroidNoPlayStoreSearchConcepts());
  registry()->RemoveSearchTags(GetAndroidPlayStoreDisabledSearchConcepts());
  registry()->RemoveSearchTags(GetAndroidPlayStoreSearchConcepts());
  registry()->RemoveSearchTags(GetAndroidSettingsSearchConcepts());

  if (!arc::IsPlayStoreAvailable()) {
    registry()->AddSearchTags(GetAndroidNoPlayStoreSearchConcepts());
    return;
  }

  if (!arc::IsArcPlayStoreEnabledForProfile(profile())) {
    registry()->AddSearchTags(GetAndroidPlayStoreDisabledSearchConcepts());
    return;
  }

  registry()->AddSearchTags(GetAndroidPlayStoreSearchConcepts());

  if (arc_app_list_prefs_ &&
      arc_app_list_prefs_->IsRegistered(arc::kSettingsAppId)) {
    registry()->AddSearchTags(GetAndroidSettingsSearchConcepts());
  }
}

}  // namespace settings
}  // namespace chromeos
