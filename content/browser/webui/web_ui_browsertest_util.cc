// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_browsertest_util.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace content {

namespace {

void GetResource(const std::string& id,
                 WebUIDataSource::GotDataCallback callback) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  if (id == "error") {
    std::move(callback).Run(nullptr);
    return;
  }

  std::string contents;
  base::FilePath path;
  CHECK(base::PathService::Get(content::DIR_TEST_DATA, &path));
  path = path.AppendASCII(id.substr(0, id.find("?")));
  CHECK(base::ReadFileToString(path, &contents)) << path.value();

  base::RefCountedString* ref_contents = new base::RefCountedString;
  ref_contents->data() = contents;
  std::move(callback).Run(ref_contents);
}

struct WebUIControllerConfig {
  int bindings = BINDINGS_POLICY_WEB_UI;
  std::string child_src = "child-src 'self' chrome://web-ui-subframe/;";
  bool disable_xfo = false;
};

class TestWebUIController : public WebUIController {
 public:
  TestWebUIController(WebUI* web_ui,
                      const GURL& base_url,
                      const WebUIControllerConfig& config)
      : WebUIController(web_ui) {
    web_ui->SetBindings(config.bindings);

    WebUIDataSource* data_source = WebUIDataSource::Create(base_url.host());
    data_source->SetRequestFilter(
        base::BindRepeating([](const std::string& path) { return true; }),
        base::BindRepeating(&GetResource));

    data_source->OverrideContentSecurityPolicyChildSrc(config.child_src);

    if (config.disable_xfo)
      data_source->DisableDenyXFrameOptions();

    WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                         data_source);
  }
  TestWebUIController(const TestWebUIController&) = delete;
  void operator=(const TestWebUIController&) = delete;
};

}  // namespace

TestWebUIControllerFactory::TestWebUIControllerFactory() = default;

std::unique_ptr<WebUIController>
TestWebUIControllerFactory::CreateWebUIControllerForURL(WebUI* web_ui,
                                                        const GURL& url) {
  if (!url.SchemeIs(kChromeUIScheme))
    return nullptr;

  WebUIControllerConfig config;
  config.disable_xfo = disable_xfo_;

  if (url.has_query()) {
    std::string value;
    bool has_value = net::GetValueForKeyInQuery(url, "bindings", &value);
    if (has_value)
      CHECK(base::StringToInt(value, &(config.bindings)));

    has_value = net::GetValueForKeyInQuery(url, "noxfo", &value);
    if (has_value && value == "true")
      config.disable_xfo = true;

    has_value = net::GetValueForKeyInQuery(url, "childsrc", &value);
    if (has_value)
      config.child_src = value;
  }

  return std::make_unique<TestWebUIController>(web_ui, url, config);
}

WebUI::TypeID TestWebUIControllerFactory::GetWebUIType(
    BrowserContext* browser_context,
    const GURL& url) {
  if (!url.SchemeIs(kChromeUIScheme))
    return WebUI::kNoWebUI;

  return reinterpret_cast<WebUI::TypeID>(base::FastHash(url.host()));
}

bool TestWebUIControllerFactory::UseWebUIForURL(BrowserContext* browser_context,
                                                const GURL& url) {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

bool TestWebUIControllerFactory::UseWebUIBindingsForURL(
    BrowserContext* browser_context,
    const GURL& url) {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

}  // namespace content
