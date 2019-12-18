// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_system_web_app_web_ui_controller_factory.h"

#include "base/memory/ref_counted_memory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

constexpr char kSystemAppManifestText[] =
    R"({
      "name": "Test System App",
      "display": "standalone",
      "icons": [
        {
          "src": "icon-256.png",
          "sizes": "256x256",
          "type": "image/png"
        }
      ],
      "start_url": "/pwa.html",
      "theme_color": "#00FF00"
    })";

constexpr char kPwaHtml[] =
    R"(
<html>
<head>
  <link rel="manifest" href="manifest.json">
</head>
</html>
)";

// WebUIController that serves a System PWA.
class TestSystemWebAppWebUIController : public content::WebUIController {
 public:
  explicit TestSystemWebAppWebUIController(std::string source_name,
                                           content::WebUI* web_ui)
      : WebUIController(web_ui) {
    content::WebUIDataSource* data_source =
        content::WebUIDataSource::Create(source_name);
    data_source->AddResourcePath("icon-256.png", IDR_PRODUCT_LOGO_256);
    data_source->SetRequestFilter(
        base::BindRepeating([](const std::string& path) {
          return path == "manifest.json" || path == "pwa.html";
        }),
        base::BindRepeating(
            [](const std::string& id,
               content::WebUIDataSource::GotDataCallback callback) {
              scoped_refptr<base::RefCountedString> ref_contents(
                  new base::RefCountedString);
              if (id == "manifest.json")
                ref_contents->data() = kSystemAppManifestText;
              else if (id == "pwa.html")
                ref_contents->data() = kPwaHtml;
              else
                NOTREACHED();

              std::move(callback).Run(ref_contents);
            }));
    content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                  data_source);
  }
  TestSystemWebAppWebUIController(const TestSystemWebAppWebUIController&) =
      delete;
  TestSystemWebAppWebUIController& operator=(
      const TestSystemWebAppWebUIController&) = delete;
};

}  // namespace

std::unique_ptr<content::WebUIController>
TestSystemWebAppWebUIControllerFactory::CreateWebUIControllerForURL(
    content::WebUI* web_ui,
    const GURL& url) {
  return std::make_unique<TestSystemWebAppWebUIController>(source_name_,
                                                           web_ui);
}

content::WebUI::TypeID TestSystemWebAppWebUIControllerFactory::GetWebUIType(
    content::BrowserContext* browser_context,
    const GURL& url) {
  if (url.SchemeIs(content::kChromeUIScheme))
    return reinterpret_cast<content::WebUI::TypeID>(1);

  return content::WebUI::kNoWebUI;
}

bool TestSystemWebAppWebUIControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme);
}

bool TestSystemWebAppWebUIControllerFactory::UseWebUIBindingsForURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme);
}
