// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/arc_graphics_tracing/arc_graphics_tracing_ui.h"

#include <memory>
#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/arc_graphics_tracing/arc_graphics_tracing_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

constexpr char kArcGraphicsTracingJsPath[] = "arc_graphics_tracing.js";
constexpr char kArcGraphicsTracingUiJsPath[] = "arc_graphics_tracing_ui.js";
constexpr char kArcGraphicsTracingCssPath[] = "arc_graphics_tracing.css";

content::WebUIDataSource* CreateDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIArcGraphicsTracingHost);
  source->UseStringsJs();
  source->SetDefaultResource(IDR_ARC_GRAPHICS_TRACING_HTML);
  source->AddResourcePath(kArcGraphicsTracingJsPath,
                          IDR_ARC_GRAPHICS_TRACING_JS);
  source->AddResourcePath(kArcGraphicsTracingUiJsPath,
                          IDR_ARC_GRAPHICS_TRACING_UI_JS);
  source->AddResourcePath(kArcGraphicsTracingCssPath,
                          IDR_ARC_GRAPHICS_TRACING_CSS);
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources 'self';");

  base::DictionaryValue localized_strings;
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &localized_strings);
  source->AddLocalizedStrings(localized_strings);

  return source;
}

}  // anonymous namespace

namespace chromeos {

ArcGraphicsTracingUI::ArcGraphicsTracingUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<ArcGraphicsTracingHandler>());
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), CreateDataSource());
}

}  // namespace chromeos
