// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/stylus_utils.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/account_manager/account_manager_util.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"
#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_handler.h"
#include "chrome/browser/ui/webui/chromeos/sync/os_sync_handler.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/settings/about_handler.h"
#include "chrome/browser/ui/webui/settings/accessibility_main_handler.h"
#include "chrome/browser/ui/webui/settings/browser_lifetime_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/accessibility_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/account_manager_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/ambient_mode_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/android_apps_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/change_picture_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/crostini_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/cups_printers_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/date_time_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_display_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_keyboard_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_pointer_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_power_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_storage_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_stylus_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/fingerprint_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/google_assistant_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/internet_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/kerberos_accounts_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/multidevice_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_features_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/parental_controls_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/plugin_vm_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/pref_names.h"
#include "chrome/browser/ui/webui/settings/chromeos/quick_unlock_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/settings_user_action_tracker.h"
#include "chrome/browser/ui/webui/settings/chromeos/wallpaper_handler.h"
#include "chrome/browser/ui/webui/settings/downloads_handler.h"
#include "chrome/browser/ui/webui/settings/extension_control_handler.h"
#include "chrome/browser/ui/webui/settings/font_handler.h"
#include "chrome/browser/ui/webui/settings/languages_handler.h"
#include "chrome/browser/ui/webui/settings/people_handler.h"
#include "chrome/browser/ui/webui/settings/profile_info_handler.h"
#include "chrome/browser/ui/webui/settings/protocol_handlers_handler.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/browser/ui/webui/settings/search_engines_handler.h"
#include "chrome/browser/ui/webui/settings/settings_cookies_view_handler.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/tts_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/os_settings_resources.h"
#include "chrome/grit/os_settings_resources_map.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "chromeos/components/web_applications/manifest_request_filter.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "chromeos/login/auth/password_visibility_utils.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/resources/grit/webui_resources.h"

namespace chromeos {
namespace settings {

// static
void OSSettingsUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kSyncOsWallpaper, false);
}

OSSettingsUI::OSSettingsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      time_when_opened_(base::TimeTicks::Now()),
      webui_load_timer_(web_ui->GetWebContents(),
                        "ChromeOS.Settings.LoadDocumentTime",
                        "ChromeOS.Settings.LoadCompletedTime") {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIOSSettingsHost);

  InitOSWebUIHandlers(html_source);

  // This handler is for chrome://os-settings.
  html_source->AddBoolean("isOSSettings", true);

  AddSettingsPageUIHandler(
      std::make_unique<::settings::AccessibilityMainHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<::settings::BrowserLifetimeHandler>());
  AddSettingsPageUIHandler(std::make_unique<::settings::CookiesViewHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<::settings::DownloadsHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::ExtensionControlHandler>());
  AddSettingsPageUIHandler(std::make_unique<::settings::FontHandler>(web_ui));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::LanguagesHandler>(web_ui));
  if (chromeos::features::IsSplitSettingsSyncEnabled())
    AddSettingsPageUIHandler(std::make_unique<OSSyncHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::PeopleHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::ProfileInfoHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::ProtocolHandlersHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<::settings::SearchEnginesHandler>(profile));
  AddSettingsPageUIHandler(
      base::WrapUnique(::settings::AboutHandler::Create(html_source, profile)));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::ResetSettingsHandler>(profile));

  // Add the metrics handler to write uma stats.
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  // Add the System Web App resources for Settings.
  if (web_app::SystemWebAppManager::IsEnabled()) {
    html_source->AddResourcePath("icon-192.png", IDR_SETTINGS_LOGO_192);
    html_source->AddResourcePath("pwa.html", IDR_PWA_HTML);
    web_app::SetManifestRequestFilter(html_source, IDR_OS_SETTINGS_MANIFEST,
                                      IDS_SETTINGS_SETTINGS);
  }

#if BUILDFLAG(OPTIMIZE_WEBUI)
  html_source->AddResourcePath("crisper.js", IDR_OS_SETTINGS_CRISPER_JS);
  html_source->AddResourcePath("lazy_load.crisper.js",
                               IDR_OS_SETTINGS_LAZY_LOAD_CRISPER_JS);
  html_source->AddResourcePath("chromeos/lazy_load.html",
                               IDR_OS_SETTINGS_LAZY_LOAD_VULCANIZED_HTML);
  html_source->SetDefaultResource(IDR_OS_SETTINGS_VULCANIZED_HTML);
#else
  // Add all settings resources.
  for (size_t i = 0; i < kOsSettingsResourcesSize; ++i) {
    html_source->AddResourcePath(kOsSettingsResources[i].name,
                                 kOsSettingsResources[i].value);
  }
  html_source->SetDefaultResource(IDR_OS_SETTINGS_SETTINGS_HTML);
#endif

  html_source->AddResourcePath("constants/routes.mojom-lite.js",
                               IDR_OS_SETTINGS_ROUTES_MOJOM_LITE_JS);
  html_source->AddResourcePath("constants/setting.mojom-lite.js",
                               IDR_OS_SETTINGS_SETTING_MOJOM_LITE_JS);

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

  html_source->AddResourcePath(
      "search/user_action_recorder.mojom-lite.js",
      IDR_OS_SETTINGS_USER_ACTION_RECORDER_MOJOM_LITE_JS);
  html_source->AddResourcePath(
      "search/search_result_icon.mojom-lite.js",
      IDR_OS_SETTINGS_SEARCH_RESULT_ICON_MOJOM_LITE_JS);
  html_source->AddResourcePath("search/search.mojom-lite.js",
                               IDR_OS_SETTINGS_SEARCH_MOJOM_LITE_JS);

  OsSettingsManagerFactory::GetForProfile(profile)->AddLoadTimeData(
      html_source);

  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString("profileLabel",
                                            IDS_OS_SETTINGS_PROFILE_LABEL);
  web_ui->AddMessageHandler(std::move(plural_string_handler));

  ManagedUIHandler::Initialize(web_ui, html_source);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source);
}

OSSettingsUI::~OSSettingsUI() {
  // Note: OSSettingsUI lifetime is tied to the lifetime of the browser window.
  base::UmaHistogramCustomTimes("ChromeOS.Settings.WindowOpenDuration",
                                base::TimeTicks::Now() - time_when_opened_,
                                /*min=*/base::TimeDelta::FromMicroseconds(500),
                                /*max=*/base::TimeDelta::FromHours(1),
                                /*buckets=*/50);
}

void OSSettingsUI::InitOSWebUIHandlers(content::WebUIDataSource* html_source) {
  Profile* profile = Profile::FromWebUI(web_ui());
  OsSettingsManagerFactory::GetForProfile(profile)->AddHandlers(web_ui());

  // TODO(jamescook): Sort out how account management is split between Chrome OS
  // and browser settings.
  if (chromeos::IsAccountManagerAvailable(profile)) {
    chromeos::AccountManagerFactory* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    chromeos::AccountManager* account_manager =
        factory->GetAccountManager(profile->GetPath().value());
    DCHECK(account_manager);

    web_ui()->AddMessageHandler(
        std::make_unique<chromeos::settings::AccountManagerUIHandler>(
            account_manager, IdentityManagerFactory::GetForProfile(profile)));
    html_source->AddBoolean(
        "secondaryGoogleAccountSigninAllowed",
        profile->GetPrefs()->GetBoolean(
            chromeos::prefs::kSecondaryGoogleAccountSigninAllowed));
    html_source->AddBoolean("isEduCoexistenceEnabled",
                            ::chromeos::features::IsEduCoexistenceEnabled());
  }

  web_ui()->AddMessageHandler(
      std::make_unique<chromeos::settings::ChangePictureHandler>());

  web_ui()->AddMessageHandler(
      std::make_unique<chromeos::settings::AccessibilityHandler>(profile));
  web_ui()->AddMessageHandler(
      std::make_unique<chromeos::settings::AndroidAppsHandler>(profile));
  if (crostini::CrostiniFeatures::Get()->IsUIAllowed(profile,
                                                     /*check_policy=*/false)) {
    web_ui()->AddMessageHandler(
        std::make_unique<chromeos::settings::CrostiniHandler>(profile));
  }
  web_ui()->AddMessageHandler(
      chromeos::settings::CupsPrintersHandler::Create(web_ui()));
  web_ui()->AddMessageHandler(base::WrapUnique(
      chromeos::settings::DateTimeHandler::Create(html_source)));
  web_ui()->AddMessageHandler(
      std::make_unique<chromeos::settings::FingerprintHandler>(profile));
  web_ui()->AddMessageHandler(
      std::make_unique<chromeos::settings::GoogleAssistantHandler>());

  std::unique_ptr<chromeos::settings::KerberosAccountsHandler>
      kerberos_accounts_handler =
          chromeos::settings::KerberosAccountsHandler::CreateIfKerberosEnabled(
              profile);
  if (kerberos_accounts_handler) {
    // Note that the UI is enabled only if Kerberos is enabled.
    web_ui()->AddMessageHandler(std::move(kerberos_accounts_handler));
  }

  if (ash::features::IsDisplayIdentificationEnabled()) {
    web_ui()->AddMessageHandler(
        std::make_unique<chromeos::settings::DisplayHandler>());
  }

  web_ui()->AddMessageHandler(
      std::make_unique<chromeos::settings::KeyboardHandler>());

  web_ui()->AddMessageHandler(
      std::make_unique<chromeos::settings::WallpaperHandler>(web_ui()));

  if (plugin_vm::IsPluginVmAllowedForProfile(profile) ||
      profile->GetPrefs()->GetBoolean(plugin_vm::prefs::kPluginVmImageExists)) {
    web_ui()->AddMessageHandler(
        std::make_unique<chromeos::settings::PluginVmHandler>(profile));
  }
  web_ui()->AddMessageHandler(
      std::make_unique<chromeos::settings::PointerHandler>());
  web_ui()->AddMessageHandler(
      std::make_unique<chromeos::settings::QuickUnlockHandler>());
  web_ui()->AddMessageHandler(
      std::make_unique<chromeos::settings::StorageHandler>(profile,
                                                           html_source));
  web_ui()->AddMessageHandler(
      std::make_unique<chromeos::settings::StylusHandler>());
  web_ui()->AddMessageHandler(
      std::make_unique<chromeos::settings::InternetHandler>(profile));
  web_ui()->AddMessageHandler(std::make_unique<::settings::TtsHandler>());
  web_ui()->AddMessageHandler(
      std::make_unique<chromeos::smb_dialog::SmbHandler>(profile,
                                                         base::DoNothing()));

  if (!profile->IsGuestSession()) {
    chromeos::android_sms::AndroidSmsService* android_sms_service =
        chromeos::android_sms::AndroidSmsServiceFactory::GetForBrowserContext(
            profile);
    web_ui()->AddMessageHandler(
        std::make_unique<chromeos::settings::MultideviceHandler>(
            profile->GetPrefs(),
            chromeos::multidevice_setup::MultiDeviceSetupClientFactory::
                GetForProfile(profile),
            android_sms_service
                ? android_sms_service->android_sms_pairing_state_tracker()
                : nullptr,
            android_sms_service ? android_sms_service->android_sms_app_manager()
                                : nullptr));
    if (features::ShouldShowParentalControlSettings(profile)) {
      web_ui()->AddMessageHandler(
          std::make_unique<chromeos::settings::ParentalControlsHandler>(
              profile));
    }

    if (chromeos::features::IsAmbientModeEnabled()) {
      web_ui()->AddMessageHandler(
          std::make_unique<chromeos::settings::AmbientModeHandler>());
    }
  }

  html_source->AddBoolean(
      "userCannotManuallyEnterPassword",
      !chromeos::password_visibility::AccountHasUserFacingPassword(
          chromeos::ProfileHelper::Get()
              ->GetUserByProfile(profile)
              ->GetAccountId()));
  html_source->AddBoolean("hasInternalStylus",
                          ash::stylus_utils::HasInternalStylus());

  html_source->AddBoolean("isDemoSession",
                          chromeos::DemoSession::IsDeviceInDemoMode());

  web_ui()->AddMessageHandler(
      std::make_unique<chromeos::settings::PowerHandler>(profile->GetPrefs()));
}

void OSSettingsUI::AddSettingsPageUIHandler(
    std::unique_ptr<content::WebUIMessageHandler> handler) {
  DCHECK(handler);
  web_ui()->AddMessageHandler(std::move(handler));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<network_config::mojom::CrosNetworkConfig> receiver) {
  ash::GetNetworkConfigService(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<mojom::UserActionRecorder> receiver) {
  user_action_recorder_ =
      std::make_unique<SettingsUserActionTracker>(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<mojom::SearchHandler> receiver) {
  SearchHandlerFactory::GetForProfile(Profile::FromWebUI(web_ui()))
      ->BindInterface(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<app_management::mojom::PageHandlerFactory> receiver) {
  if (!app_management_page_handler_factory_) {
    app_management_page_handler_factory_ =
        std::make_unique<AppManagementPageHandlerFactory>(
            Profile::FromWebUI(web_ui()));
  }
  app_management_page_handler_factory_->Bind(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(OSSettingsUI)

}  // namespace settings
}  // namespace chromeos
