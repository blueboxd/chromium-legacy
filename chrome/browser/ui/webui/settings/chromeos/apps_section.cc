// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/apps_section.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/android_apps_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
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
      {"appManagementSearchPrompt", IDS_APP_MANAGEMENT_SEARCH_PROMPT},
      {"appManagementStoragePermissionLabel", IDS_APP_MANAGEMENT_STORAGE},
      {"appManagementUninstallLabel", IDS_APP_MANAGEMENT_UNINSTALL_APP},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
}

}  // namespace

AppsSection::AppsSection(Profile* profile,
                         Delegate* per_page_delegate,
                         PrefService* pref_service,
                         ArcAppListPrefs* arc_app_list_prefs)
    : OsSettingsSection(profile, per_page_delegate),
      pref_service_(pref_service),
      arc_app_list_prefs_(arc_app_list_prefs) {
  delegate()->AddSearchTags(GetAppsSearchConcepts());

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
}

void AppsSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::AndroidAppsHandler>(profile()));
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

void AppsSection::UpdateAndroidSearchTags() {
  delegate()->RemoveSearchTags(GetAndroidNoPlayStoreSearchConcepts());
  delegate()->RemoveSearchTags(GetAndroidPlayStoreDisabledSearchConcepts());
  delegate()->RemoveSearchTags(GetAndroidPlayStoreSearchConcepts());
  delegate()->RemoveSearchTags(GetAndroidSettingsSearchConcepts());

  if (!arc::IsPlayStoreAvailable()) {
    delegate()->AddSearchTags(GetAndroidNoPlayStoreSearchConcepts());
    return;
  }

  if (!arc::IsArcPlayStoreEnabledForProfile(profile())) {
    delegate()->AddSearchTags(GetAndroidPlayStoreDisabledSearchConcepts());
    return;
  }

  delegate()->AddSearchTags(GetAndroidPlayStoreSearchConcepts());

  if (arc_app_list_prefs_ &&
      arc_app_list_prefs_->IsRegistered(arc::kSettingsAppId)) {
    delegate()->AddSearchTags(GetAndroidSettingsSearchConcepts());
  }
}

}  // namespace settings
}  // namespace chromeos
