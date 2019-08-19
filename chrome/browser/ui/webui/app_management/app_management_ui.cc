// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_ui.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

content::WebUIDataSource* CreateAppManagementUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIAppManagementHost);

  static constexpr LocalizedString kStrings[] = {
      {"appListTitle", IDS_APP_MANAGEMENT_APP_LIST_TITLE},
      {"appNoPermission", IDS_APPLICATION_INFO_APP_NO_PERMISSIONS_TEXT},
      {"back", IDS_APP_MANAGEMENT_BACK},
      {"camera", IDS_APP_MANAGEMENT_CAMERA},
      {"contacts", IDS_APP_MANAGEMENT_CONTACTS},
      {"controlledByPolicy", IDS_CONTROLLED_SETTING_POLICY},
      {"lessApps", IDS_APP_MANAGEMENT_LESS_APPS},
      {"location", IDS_APP_MANAGEMENT_LOCATION},
      {"microphone", IDS_APP_MANAGEMENT_MICROPHONE},
      {"moreApps", IDS_APP_MANAGEMENT_MORE_APPS},
      {"moreSettings", IDS_APP_MANAGEMENT_MORE_SETTINGS},
      {"noSearchResults", IDS_APP_MANAGEMENT_NO_RESULTS},
      {"notifications", IDS_APP_MANAGEMENT_NOTIFICATIONS},
      {"notificationSublabel", IDS_APP_MANAGEMENT_NOTIFICATIONS_SUBLABEL},
      {"openAndroidSettings", IDS_APP_MANAGEMENT_ANDROID_SETTINGS},
      {"openExtensionsSettings", IDS_APP_MANAGEMENT_EXTENSIONS_SETTINGS},
      {"openSiteSettings", IDS_APP_MANAGEMENT_SITE_SETTING},
      {"permissions", IDS_APP_MANAGEMENT_PERMISSIONS},
      {"pinControlledByPolicy", IDS_APP_MANAGEMENT_PIN_ENFORCED_BY_POLICY},
      {"pinToShelf", IDS_APP_MANAGEMENT_PIN_TO_SHELF},
      {"searchPrompt", IDS_APP_MANAGEMENT_SEARCH_PROMPT},
      {"size", IDS_APP_MANAGEMENT_SIZE},
      {"storage", IDS_APP_MANAGEMENT_STORAGE},
      {"thisAppCan", IDS_APP_MANAGEMENT_THIS_APP_CAN},
      {"title", IDS_APP_MANAGEMENT_TITLE},
      {"uninstall", IDS_APP_MANAGEMENT_UNINSTALL},
      {"version", IDS_APP_MANAGEMENT_VERSION},
  };
  AddLocalizedStringsBulk(source, kStrings, base::size(kStrings));

#if defined(OS_CHROMEOS)
  source->AddBoolean(
      "isSupportedArcVersion",
      AppManagementPageHandler::IsCurrentArcVersionSupported(profile));
#endif  // OS_CHROMEOS

  source->AddResourcePath("app_management.mojom-lite.js",
                          IDR_APP_MANAGEMENT_MOJO_LITE_JS);
  source->AddResourcePath("types.mojom-lite.js",
                          IDR_APP_MANAGEMENT_TYPES_MOJO_LITE_JS);
  source->AddResourcePath("bitmap.mojom-lite.js",
                          IDR_APP_MANAGEMENT_BITMAP_MOJO_LITE_JS);
  source->AddResourcePath("image.mojom-lite.js",
                          IDR_APP_MANAGEMENT_IMAGE_MOJO_LITE_JS);
  source->AddResourcePath("image_info.mojom-lite.js",
                          IDR_APP_MANAGEMENT_IMAGE_INFO_MOJO_LITE_JS);

  source->AddResourcePath("app.html", IDR_APP_MANAGEMENT_APP_HTML);
  source->AddResourcePath("app.js", IDR_APP_MANAGEMENT_APP_JS);
  source->AddResourcePath("expandable_app_list.html",
                          IDR_APP_MANAGEMENT_EXPANDABLE_APP_LIST_HTML);
  source->AddResourcePath("expandable_app_list.js",
                          IDR_APP_MANAGEMENT_EXPANDABLE_APP_LIST_JS);

  source->SetDefaultResource(IDR_APP_MANAGEMENT_INDEX_HTML);
  source->UseStringsJs();

  return source;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// AppManagementUI
//
///////////////////////////////////////////////////////////////////////////////

AppManagementUI::AppManagementUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true), page_factory_binding_(this) {
  Profile* profile = Profile::FromWebUI(web_ui);

  // Set up the data source.
  content::WebUIDataSource* source = CreateAppManagementUIHTMLSource(profile);
  content::WebUIDataSource::Add(profile, source);

  AddHandlerToRegistry(base::BindRepeating(
      &AppManagementUI::BindPageHandlerFactory, base::Unretained(this)));

  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString(
      "appListPreview", IDS_APP_MANAGEMENT_APP_LIST_PREVIEW);
  web_ui->AddMessageHandler(std::move(plural_string_handler));
}

AppManagementUI::~AppManagementUI() = default;

bool AppManagementUI::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kAppManagement);
}

void AppManagementUI::BindPageHandlerFactory(
    app_management::mojom::PageHandlerFactoryRequest request) {
  if (page_factory_binding_.is_bound()) {
    page_factory_binding_.Unbind();
  }

  page_factory_binding_.Bind(std::move(request));
}

void AppManagementUI::CreatePageHandler(
    app_management::mojom::PagePtr page,
    app_management::mojom::PageHandlerRequest request) {
  DCHECK(page);

  page_handler_ = std::make_unique<AppManagementPageHandler>(
      std::move(request), std::move(page), Profile::FromWebUI(web_ui()));
}
