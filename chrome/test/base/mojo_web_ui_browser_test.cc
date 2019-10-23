// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/mojo_web_ui_browser_test.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/web_ui_test_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/data/grit/webui_test_resources.h"
#include "chrome/test/data/webui/web_ui_test.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Handles Mojo-style test communication.
class WebUITestPageHandler : public web_ui_test::mojom::TestRunner,
                             public WebUITestHandler {
 public:
  explicit WebUITestPageHandler(content::WebUI* web_ui) : web_ui_(web_ui) {}
  ~WebUITestPageHandler() override {}

  // Binds the Mojo test interface to this handler.
  void BindToTestRunnerReceiver(
      mojo::PendingReceiver<web_ui_test::mojom::TestRunner> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  // web_ui_test::mojom::TestRunner:
  void TestComplete(const base::Optional<std::string>& message) override {
    WebUITestHandler::TestComplete(message);
  }

  content::WebUI* GetWebUI() override { return web_ui_; }

 private:
  content::WebUI* web_ui_;
  mojo::Receiver<web_ui_test::mojom::TestRunner> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(WebUITestPageHandler);
};

}  // namespace

MojoWebUIBrowserTest::MojoWebUIBrowserTest() {
  registry_.AddInterface<web_ui_test::mojom::TestRunner>(base::BindRepeating(
      &MojoWebUIBrowserTest::BindTestRunner, base::Unretained(this)));
}

MojoWebUIBrowserTest::~MojoWebUIBrowserTest() = default;

void MojoWebUIBrowserTest::SetUpOnMainThread() {
  BaseWebUIBrowserTest::SetUpOnMainThread();

  base::FilePath pak_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &pak_path));
  pak_path = pak_path.AppendASCII("browser_tests.pak");
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_path, ui::SCALE_FACTOR_NONE);
}

void MojoWebUIBrowserTest::OnInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  // Right now, this is expected to be called only for main frames.
  if (render_frame_host->GetParent()) {
    FAIL() << "Terminating renderer for requesting " << interface_name
           << " interface from subframe";
    render_frame_host->GetProcess()->ShutdownForBadMessage(
        content::RenderProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);
    return;
  }
  registry_.TryBindInterface(interface_name, interface_pipe);
}

void MojoWebUIBrowserTest::BindTestRunner(
    mojo::PendingReceiver<web_ui_test::mojom::TestRunner> receiver) {
  test_page_handler_->BindToTestRunnerReceiver(std::move(receiver));
}

void MojoWebUIBrowserTest::SetupHandlers() {
  content::WebUI* web_ui_instance =
      override_selected_web_ui()
          ? override_selected_web_ui()
          : browser()->tab_strip_model()->GetActiveWebContents()->GetWebUI();
  ASSERT_TRUE(web_ui_instance != nullptr);

  auto test_handler = std::make_unique<WebUITestPageHandler>(web_ui_instance);
  test_page_handler_ = test_handler.get();
  set_test_handler(std::move(test_handler));

  Observe(web_ui_instance->GetWebContents());
}

void MojoWebUIBrowserTest::BrowsePreload(const GURL& browse_to) {
  BaseWebUIBrowserTest::BrowsePreload(browse_to);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  if (use_mojo_lite_bindings_) {
    web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
        l10n_util::GetStringUTF16(IDR_WEB_UI_TEST_MOJO_LITE_JS),
        base::NullCallback());
  } else {
    web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
        l10n_util::GetStringUTF16(IDR_WEB_UI_TEST_MOJO_JS),
        base::NullCallback());
  }
}
