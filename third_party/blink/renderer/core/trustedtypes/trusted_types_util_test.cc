// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html_or_trusted_script_or_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

void TrustedTypesCheckForHTMLThrows(
    const StringOrTrustedHTML& string_or_trusted_html) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.GetContentSecurityPolicy()->DidReceiveHeader(
      "trusted-types *", network::mojom::ContentSecurityPolicyType::kEnforce,
      network::mojom::ContentSecurityPolicySource::kMeta);
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(exception_state.HadException());
  String s = TrustedTypesCheckForHTML(string_or_trusted_html, &document,
                                      exception_state);
  EXPECT_FALSE(exception_state.HadException());

  document.GetContentSecurityPolicy()->DidReceiveHeader(
      "require-trusted-types-for 'script'",
      network::mojom::ContentSecurityPolicyType::kEnforce,
      network::mojom::ContentSecurityPolicySource::kMeta);
  ASSERT_FALSE(exception_state.HadException());
  String s1 = TrustedTypesCheckForHTML(string_or_trusted_html, &document,
                                       exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  exception_state.ClearException();
}

void TrustedTypesCheckForScriptThrows(
    const StringOrTrustedScript& string_or_trusted_script) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.GetContentSecurityPolicy()->DidReceiveHeader(
      "trusted-types *", network::mojom::ContentSecurityPolicyType::kEnforce,
      network::mojom::ContentSecurityPolicySource::kMeta);
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(exception_state.HadException());
  String s = TrustedTypesCheckForScript(
      string_or_trusted_script, document.ToExecutionContext(), exception_state);
  EXPECT_FALSE(exception_state.HadException());

  document.GetContentSecurityPolicy()->DidReceiveHeader(
      "require-trusted-types-for 'script'",
      network::mojom::ContentSecurityPolicyType::kEnforce,
      network::mojom::ContentSecurityPolicySource::kMeta);
  ASSERT_FALSE(exception_state.HadException());
  String s1 = TrustedTypesCheckForScript(
      string_or_trusted_script, document.ToExecutionContext(), exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  exception_state.ClearException();
}

void TrustedTypesCheckForScriptURLThrows(
    const StringOrTrustedScriptURL& string_or_trusted_script_url) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.GetContentSecurityPolicy()->DidReceiveHeader(
      "trusted-types *", network::mojom::ContentSecurityPolicyType::kEnforce,
      network::mojom::ContentSecurityPolicySource::kMeta);
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(exception_state.HadException());
  String s = TrustedTypesCheckForScriptURL(string_or_trusted_script_url,
                                           document.ToExecutionContext(),
                                           exception_state);
  EXPECT_FALSE(exception_state.HadException());

  document.GetContentSecurityPolicy()->DidReceiveHeader(
      "require-trusted-types-for 'script'",
      network::mojom::ContentSecurityPolicyType::kEnforce,
      network::mojom::ContentSecurityPolicySource::kMeta);
  ASSERT_FALSE(exception_state.HadException());
  String s1 = TrustedTypesCheckForScriptURL(string_or_trusted_script_url,
                                            document.ToExecutionContext(),
                                            exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  exception_state.ClearException();
}

void TrustedTypesCheckForHTMLWorks(
    const StringOrTrustedHTML& string_or_trusted_html,
    String expected) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.GetContentSecurityPolicy()->DidReceiveHeader(
      "trusted-types *", network::mojom::ContentSecurityPolicyType::kEnforce,
      network::mojom::ContentSecurityPolicySource::kMeta);
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  String s = TrustedTypesCheckForHTML(string_or_trusted_html, &document,
                                      exception_state);
  ASSERT_EQ(s, expected);
}

void TrustedTypesCheckForScriptWorks(
    const StringOrTrustedScript& string_or_trusted_script,
    String expected) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.GetContentSecurityPolicy()->DidReceiveHeader(
      "trusted-types *", network::mojom::ContentSecurityPolicyType::kEnforce,
      network::mojom::ContentSecurityPolicySource::kMeta);
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  String s = TrustedTypesCheckForScript(
      string_or_trusted_script, document.ToExecutionContext(), exception_state);
  ASSERT_EQ(s, expected);
}

void TrustedTypesCheckForScriptURLWorks(
    const StringOrTrustedScriptURL& string_or_trusted_script_url,
    String expected) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.GetContentSecurityPolicy()->DidReceiveHeader(
      "trusted-types *", network::mojom::ContentSecurityPolicyType::kEnforce,
      network::mojom::ContentSecurityPolicySource::kMeta);
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  String s = TrustedTypesCheckForScriptURL(string_or_trusted_script_url,
                                           document.ToExecutionContext(),
                                           exception_state);
  ASSERT_EQ(s, expected);
}

// TrustedTypesCheckForHTML tests
TEST(TrustedTypesUtilTest, TrustedTypesCheckForHTML_TrustedHTML) {
  auto* html = MakeGarbageCollected<TrustedHTML>("A string");
  StringOrTrustedHTML trusted_value =
      StringOrTrustedHTML::FromTrustedHTML(html);
  TrustedTypesCheckForHTMLWorks(trusted_value, "A string");
}

TEST(TrustedTypesUtilTest, TrustedTypesCheckForHTML_String) {
  StringOrTrustedHTML string_value =
      StringOrTrustedHTML::FromString("A string");
  TrustedTypesCheckForHTMLThrows(string_value);
}

// TrustedTypesCheckForScript tests
TEST(TrustedTypesUtilTest, TrustedTypesCheckForScript_TrustedScript) {
  auto* script = MakeGarbageCollected<TrustedScript>("A string");
  StringOrTrustedScript trusted_value =
      StringOrTrustedScript::FromTrustedScript(script);
  TrustedTypesCheckForScriptWorks(trusted_value, "A string");
}

TEST(TrustedTypesUtilTest, TrustedTypesCheckForScript_String) {
  StringOrTrustedScript string_value =
      StringOrTrustedScript::FromString("A string");
  TrustedTypesCheckForScriptThrows(string_value);
}

// TrustedTypesCheckForScriptURL tests
TEST(TrustedTypesUtilTest, TrustedTypesCheckForScriptURL_TrustedScriptURL) {
  String url_address = "http://www.example.com/";
  auto* script_url = MakeGarbageCollected<TrustedScriptURL>(url_address);
  StringOrTrustedScriptURL trusted_value =
      StringOrTrustedScriptURL::FromTrustedScriptURL(script_url);
  TrustedTypesCheckForScriptURLWorks(trusted_value, "http://www.example.com/");
}

TEST(TrustedTypesUtilTest, TrustedTypesCheckForScriptURL_String) {
  StringOrTrustedScriptURL string_value =
      StringOrTrustedScriptURL::FromString("A string");
  TrustedTypesCheckForScriptURLThrows(string_value);
}
}  // namespace blink
