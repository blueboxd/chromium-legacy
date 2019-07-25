// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/settings/about_handler.h"
#include "chrome/browser/ui/webui/settings/accessibility_main_handler.h"
#include "chrome/browser/ui/webui/settings/appearance_handler.h"
#include "chrome/browser/ui/webui/settings/browser_lifetime_handler.h"
#include "chrome/browser/ui/webui/settings/downloads_handler.h"
#include "chrome/browser/ui/webui/settings/extension_control_handler.h"
#include "chrome/browser/ui/webui/settings/languages_handler.h"
#include "chrome/browser/ui/webui/settings/people_handler.h"
#include "chrome/browser/ui/webui/settings/profile_info_handler.h"
#include "chrome/browser/ui/webui/settings/protocol_handlers_handler.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/browser/ui/webui/settings/search_engines_handler.h"
#include "chrome/browser/ui/webui/settings/settings_cookies_view_handler.h"
#include "chrome/browser/ui/webui/settings/settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/settings_media_devices_selection_handler.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/os_settings_resources.h"
#include "chrome/grit/os_settings_resources_map.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/unified_consent/feature.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace settings {

OSSettingsUI::OSSettingsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send =*/true) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIOSSettingsHost);

  ::settings::SettingsUI::InitOSWebUIHandlers(profile, web_ui, html_source);

  // This handler is for chrome://os-settings.
  html_source->AddBoolean("isOSSettings", true);

  // Needed for JS code shared between browser and OS settings (for example,
  // page_visibility.js).
  html_source->AddBoolean("showOSSettings", true);

  AddSettingsPageUIHandler(
      std::make_unique<::settings::AppearanceHandler>(web_ui));

  AddSettingsPageUIHandler(
      std::make_unique<::settings::AccessibilityMainHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<::settings::BrowserLifetimeHandler>());
  AddSettingsPageUIHandler(std::make_unique<::settings::CookiesViewHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<::settings::DownloadsHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::ExtensionControlHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<::settings::LanguagesHandler>(web_ui));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::MediaDevicesSelectionHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::PeopleHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::ProfileInfoHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::ProtocolHandlersHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<::settings::SearchEnginesHandler>(profile));

  html_source->AddBoolean("unifiedConsentEnabled",
                          unified_consent::IsUnifiedConsentFeatureEnabled());

  AddSettingsPageUIHandler(
      base::WrapUnique(::settings::AboutHandler::Create(html_source, profile)));
  AddSettingsPageUIHandler(base::WrapUnique(
      ::settings::ResetSettingsHandler::Create(html_source, profile)));

  // Add the metrics handler to write uma stats.
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  // Add the System Web App resources for Settings.
  // TODO(jamescook|calamity): Migrate to chromeos::settings::OSSettingsUI.
  if (web_app::SystemWebAppManager::IsEnabled()) {
    html_source->AddResourcePath("icon-192.png", IDR_SETTINGS_LOGO_192);
    html_source->AddResourcePath("pwa.html", IDR_PWA_HTML);
    html_source->AddResourcePath("manifest.json", IDR_OS_SETTINGS_MANIFEST);
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

  ::settings::AddLocalizedStrings(html_source, profile);

  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString("profileLabel",
                                            IDS_OS_SETTINGS_PROFILE_LABEL);
  web_ui->AddMessageHandler(std::move(plural_string_handler));

  ManagedUIHandler::Initialize(web_ui, html_source);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source);

  AddHandlerToRegistry(base::BindRepeating(&OSSettingsUI::BindCrosNetworkConfig,
                                           base::Unretained(this)));
}

OSSettingsUI::~OSSettingsUI() = default;

void OSSettingsUI::AddSettingsPageUIHandler(
    std::unique_ptr<content::WebUIMessageHandler> handler) {
  DCHECK(handler);
  web_ui()->AddMessageHandler(std::move(handler));
}

void OSSettingsUI::BindCrosNetworkConfig(
    network_config::mojom::CrosNetworkConfigRequest request) {
  content::BrowserContext::GetConnectorFor(
      web_ui()->GetWebContents()->GetBrowserContext())
      ->BindInterface(network_config::mojom::kServiceName, std::move(request));
}

}  // namespace settings
}  // namespace chromeos
