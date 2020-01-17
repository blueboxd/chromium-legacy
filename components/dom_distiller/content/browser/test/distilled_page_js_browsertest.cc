// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/content/browser/test/test_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/scale_factor.h"

namespace dom_distiller {
namespace {

base::Value ExecuteJsScript(content::WebContents* web_contents,
                            const std::string& script) {
  base::Value result;
  base::RunLoop run_loop;
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(script),
      base::BindOnce(
          [](base::Closure callback, base::Value* out, base::Value result) {
            (*out) = std::move(result);
            callback.Run();
          },
          run_loop.QuitClosure(), &result));
  run_loop.Run();
  return result;
}

class DistilledPageJsTest : public content::ContentBrowserTest {
 protected:
  explicit DistilledPageJsTest()
      : content::ContentBrowserTest(), distilled_page_(nullptr) {}
  ~DistilledPageJsTest() override = default;

  void SetUpOnMainThread() override {
    if (!DistillerJavaScriptWorldIdIsSet()) {
      SetDistillerJavaScriptWorldId(content::ISOLATED_WORLD_ID_CONTENT_END);
    }

    AddComponentsResources();
    distilled_page_ = SetUpTestServerWithDistilledPage(embedded_test_server());
  }

  void LoadAndExecuteTestScript(const std::string& file,
                                const std::string& fixture_name) {
    distilled_page_->AppendScriptFile(file);
    distilled_page_->Load(embedded_test_server(), shell()->web_contents());
    const base::Value result = ExecuteJsScript(
        shell()->web_contents(), base::StrCat({fixture_name, ".run()"}));
    ASSERT_EQ(base::Value::Type::BOOLEAN, result.type());
    EXPECT_TRUE(result.GetBool());
  }

  std::unique_ptr<FakeDistilledPage> distilled_page_;
};

#if defined(OS_WIN)
#define MAYBE_Pinch DISABLED_Pinch
#else
#define MAYBE_Pinch Pinch
#endif
IN_PROC_BROWSER_TEST_F(DistilledPageJsTest, MAYBE_Pinch) {
  LoadAndExecuteTestScript("pinch_tester.js", "pinchtest");
}

}  // namespace
}  // namespace dom_distiller
