// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_util.h"

#include "chrome/common/buildflags.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/resources/grit/webui_resources_map.h"

namespace webui {

namespace {

void SetupPolymer3Defaults(content::WebUIDataSource* source) {
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources chrome://test 'self';");
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER);
}

}  // namespace

void SetupWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const GritResourceMap> resources,
                          const std::string& generated_path,
                          int default_resource) {
  SetupPolymer3Defaults(source);
  for (const GritResourceMap& resource : resources) {
    std::string path = resource.name;
    if (path.rfind(generated_path, 0) == 0) {
      path = path.substr(generated_path.size());
    }

    source->AddResourcePath(path, resource.value);
  }
  source->SetDefaultResource(default_resource);
}

#if BUILDFLAG(OPTIMIZE_WEBUI)
void SetupBundledWebUIDataSource(content::WebUIDataSource* source,
                                 base::StringPiece bundled_path,
                                 int bundle,
                                 int default_resource) {
  SetupPolymer3Defaults(source);
  source->AddResourcePath(bundled_path, bundle);
  source->SetDefaultResource(default_resource);
}
#endif

void AddLocalizedStringsBulk(content::WebUIDataSource* html_source,
                             base::span<const LocalizedString> strings) {
  for (const auto& str : strings)
    html_source->AddLocalizedString(str.name, str.id);
}

void AddResourcePathsBulk(content::WebUIDataSource* source,
                          base::span<const ResourcePath> paths) {
  for (const auto& path : paths)
    source->AddResourcePath(path.path, path.id);
}

}  // namespace webui
