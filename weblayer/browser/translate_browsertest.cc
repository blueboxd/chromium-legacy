// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/translate_waiter.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_error_details.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/common/translate_switches.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/browser/translate_client_impl.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

namespace {

static std::string kTestValidScript = R"(
    var google = {};
    google.translate = (function() {
      return {
        TranslateService: function() {
          return {
            isAvailable : function() {
              return true;
            },
            restore : function() {
              return;
            },
            getDetectedLanguage : function() {
              return "fr";
            },
            translatePage : function(originalLang, targetLang,
                                     onTranslateProgress) {
              var error = (originalLang == 'auto') ? true : false;
              onTranslateProgress(100, true, error);
            }
          };
        }
      };
    })();
    cr.googleTranslate.onTranslateElementLoad();)";

static std::string kTestScriptInitializationError = R"(
    var google = {};
    google.translate = (function() {
      return {
        TranslateService: function() {
          return error;
        }
      };
    })();
    cr.googleTranslate.onTranslateElementLoad();)";

static std::string kTestScriptTimeout = R"(
    var google = {};
    google.translate = (function() {
      return {
        TranslateService: function() {
          return {
            isAvailable : function() {
              return false;
            },
          };
        }
      };
    })();
    cr.googleTranslate.onTranslateElementLoad();)";

TranslateClientImpl* GetTranslateClient(Shell* shell) {
  return TranslateClientImpl::FromWebContents(
      static_cast<TabImpl*>(shell->tab())->web_contents());
}

std::unique_ptr<translate::TranslateWaiter> CreateTranslateWaiter(
    Shell* shell,
    translate::TranslateWaiter::WaitEvent wait_event) {
  return std::make_unique<translate::TranslateWaiter>(
      GetTranslateClient(shell)->translate_driver(), wait_event);
}

void WaitUntilLanguageDetermined(Shell* shell) {
  CreateTranslateWaiter(
      shell, translate::TranslateWaiter::WaitEvent::kLanguageDetermined)
      ->Wait();
}

void WaitUntilPageTranslated(Shell* shell) {
  CreateTranslateWaiter(shell,
                        translate::TranslateWaiter::WaitEvent::kPageTranslated)
      ->Wait();
}

}  // namespace

class TranslateBrowserTest : public WebLayerBrowserTest {
 public:
  TranslateBrowserTest() {
    error_subscription_ =
        translate::TranslateManager::RegisterTranslateErrorCallback(
            base::BindRepeating(&TranslateBrowserTest::OnTranslateError,
                                base::Unretained(this)));
  }
  ~TranslateBrowserTest() override = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &TranslateBrowserTest::HandleRequest, base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    command_line->AppendSwitchASCII(
        translate::switches::kTranslateScriptURL,
        embedded_test_server()->GetURL("/mock_translate_script.js").spec());
  }

 protected:
  translate::TranslateErrors::Type GetPageTranslatedResult() {
    return error_type_;
  }
  void SetTranslateScript(const std::string& script) { script_ = script; }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path() != "/mock_translate_script.js")
      return nullptr;

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(script_);
    http_response->set_content_type("text/javascript");
    return std::move(http_response);
  }

  void OnTranslateError(const translate::TranslateErrorDetails& details) {
    error_type_ = details.error;
  }

  translate::TranslateErrors::Type error_type_ =
      translate::TranslateErrors::NONE;
  std::unique_ptr<
      translate::TranslateManager::TranslateErrorCallbackList::Subscription>
      error_subscription_;
  std::string script_;
};

// Tests that the CLD (Compact Language Detection) works properly.
IN_PROC_BROWSER_TEST_F(TranslateBrowserTest, PageLanguageDetection) {
  TranslateClientImpl* translate_client = GetTranslateClient(shell());

  NavigateAndWaitForCompletion(GURL("about:blank"), shell());
  WaitUntilLanguageDetermined(shell());
  EXPECT_EQ("und", translate_client->GetLanguageState().original_language());

  // Go to a page in English.
  NavigateAndWaitForCompletion(
      GURL(embedded_test_server()->GetURL("/english_page.html")), shell());
  WaitUntilLanguageDetermined(shell());
  EXPECT_EQ("en", translate_client->GetLanguageState().original_language());

  // Now navigate to a page in French.
  NavigateAndWaitForCompletion(
      GURL(embedded_test_server()->GetURL("/french_page.html")), shell());
  WaitUntilLanguageDetermined(shell());
  EXPECT_EQ("fr", translate_client->GetLanguageState().original_language());
}

// Test that the translation was successful.
IN_PROC_BROWSER_TEST_F(TranslateBrowserTest, PageTranslationSuccess) {
  SetTranslateScript(kTestValidScript);

  TranslateClientImpl* translate_client = GetTranslateClient(shell());

  NavigateAndWaitForCompletion(GURL("about:blank"), shell());
  WaitUntilLanguageDetermined(shell());
  EXPECT_EQ("und", translate_client->GetLanguageState().original_language());

  // Navigate to a page in French.
  NavigateAndWaitForCompletion(
      GURL(embedded_test_server()->GetURL("/french_page.html")), shell());
  WaitUntilLanguageDetermined(shell());
  EXPECT_EQ("fr", translate_client->GetLanguageState().original_language());

  // Translate the page through TranslateManager.
  translate::TranslateManager* manager =
      translate_client->GetTranslateManager();
  manager->TranslatePage(
      translate_client->GetLanguageState().original_language(), "en", true);

  WaitUntilPageTranslated(shell());

  EXPECT_FALSE(translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(translate::TranslateErrors::NONE, GetPageTranslatedResult());
}

// Test if there was an error during translation.
IN_PROC_BROWSER_TEST_F(TranslateBrowserTest, PageTranslationError) {
  SetTranslateScript(kTestValidScript);

  TranslateClientImpl* translate_client = GetTranslateClient(shell());

  NavigateAndWaitForCompletion(GURL("about:blank"), shell());
  WaitUntilLanguageDetermined(shell());
  EXPECT_EQ("und", translate_client->GetLanguageState().original_language());

  // Translate the page through TranslateManager.
  translate::TranslateManager* manager =
      translate_client->GetTranslateManager();
  manager->TranslatePage(
      translate_client->GetLanguageState().original_language(), "en", true);

  WaitUntilPageTranslated(shell());

  EXPECT_TRUE(translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(translate::TranslateErrors::TRANSLATION_ERROR,
            GetPageTranslatedResult());
}

// Test if there was an error during translate library initialization.
IN_PROC_BROWSER_TEST_F(TranslateBrowserTest,
                       PageTranslationInitializationError) {
  SetTranslateScript(kTestScriptInitializationError);

  TranslateClientImpl* translate_client = GetTranslateClient(shell());

  NavigateAndWaitForCompletion(GURL("about:blank"), shell());
  WaitUntilLanguageDetermined(shell());
  EXPECT_EQ("und", translate_client->GetLanguageState().original_language());

  // Navigate to a page in French.
  NavigateAndWaitForCompletion(
      GURL(embedded_test_server()->GetURL("/french_page.html")), shell());
  WaitUntilLanguageDetermined(shell());
  EXPECT_EQ("fr", translate_client->GetLanguageState().original_language());

  // Translate the page through TranslateManager.
  translate::TranslateManager* manager =
      translate_client->GetTranslateManager();
  manager->TranslatePage(
      translate_client->GetLanguageState().original_language(), "en", true);

  WaitUntilPageTranslated(shell());

  EXPECT_TRUE(translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(translate::TranslateErrors::INITIALIZATION_ERROR,
            GetPageTranslatedResult());
}

// Test the checks translate lib never gets ready and throws timeout.
IN_PROC_BROWSER_TEST_F(TranslateBrowserTest, PageTranslationTimeoutError) {
  SetTranslateScript(kTestScriptTimeout);

  TranslateClientImpl* translate_client = GetTranslateClient(shell());

  NavigateAndWaitForCompletion(GURL("about:blank"), shell());
  WaitUntilLanguageDetermined(shell());
  EXPECT_EQ("und", translate_client->GetLanguageState().original_language());

  // Navigate to a page in French.
  NavigateAndWaitForCompletion(
      GURL(embedded_test_server()->GetURL("/french_page.html")), shell());
  WaitUntilLanguageDetermined(shell());
  EXPECT_EQ("fr", translate_client->GetLanguageState().original_language());

  // Translate the page through TranslateManager.
  translate::TranslateManager* manager =
      translate_client->GetTranslateManager();
  manager->TranslatePage(
      translate_client->GetLanguageState().original_language(), "en", true);

  WaitUntilPageTranslated(shell());

  EXPECT_TRUE(translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(translate::TranslateErrors::TRANSLATION_TIMEOUT,
            GetPageTranslatedResult());
}

}  // namespace weblayer
