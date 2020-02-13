// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/launch_service/app_utils.h"

#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace apps {

std::string GetAppIdForWebContents(content::WebContents* web_contents) {
  std::string app_id;

  web_app::WebAppTabHelperBase* web_app_tab_helper =
      web_app::WebAppTabHelperBase::FromWebContents(web_contents);
  // web_app_tab_helper is nullptr in some unit tests.
  if (web_app_tab_helper)
    app_id = web_app_tab_helper->GetAppId();

  if (app_id.empty()) {
    extensions::TabHelper* extensions_tab_helper =
        extensions::TabHelper::FromWebContents(web_contents);
    // extensions_tab_helper is nullptr in some tests.
    if (extensions_tab_helper)
      app_id = extensions_tab_helper->GetExtensionAppId();
  }

  return app_id;
}

bool IsInstalledApp(Profile* profile, const std::string& app_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  if (extension && !extension->from_bookmark()) {
    DCHECK(extension->is_app());
    return true;
  }
  web_app::AppRegistrar& registrar =
      web_app::WebAppProviderBase::GetProviderBase(profile)->registrar();
  return registrar.IsInstalled(app_id);
}

void SetAppIdForWebContents(Profile* profile,
                            content::WebContents* web_contents,
                            const std::string& app_id) {
  extensions::TabHelper::CreateForWebContents(web_contents);
  web_app::WebAppTabHelper::CreateForWebContents(web_contents);
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  if (extension && !extension->from_bookmark()) {
    DCHECK(extension->is_app());
    web_app::WebAppTabHelperBase::FromWebContents(web_contents)
        ->SetAppId(std::string());
    extensions::TabHelper::FromWebContents(web_contents)
        ->SetExtensionAppById(app_id);
  } else {
    web_app::AppRegistrar& registrar =
        web_app::WebAppProviderBase::GetProviderBase(profile)->registrar();
    web_app::WebAppTabHelperBase::FromWebContents(web_contents)
        ->SetAppId(registrar.IsInstalled(app_id) ? app_id : std::string());
    extensions::TabHelper::FromWebContents(web_contents)
        ->SetExtensionAppById(std::string());
  }
}

}  // namespace apps
