// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "chrome/test/chromedriver/commands.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/window_commands.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockChrome : public StubChrome {
 public:
  MockChrome() : web_view_("1") {}
  ~MockChrome() override {}

  Status GetWebViewById(const std::string& id, WebView** web_view) override {
    if (id == web_view_.GetId()) {
      *web_view = &web_view_;
      return Status(kOk);
    }
    return Status(kUnknownError);
  }

 private:
  // Using a StubWebView does not allow testing the functionality end-to-end,
  // more details in crbug.com/850703
  StubWebView web_view_;
};

typedef Status (*Command)(Session* session,
                          WebView* web_view,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout);

Status CallWindowCommand(Command command,
                         const base::DictionaryValue& params = {},
                         std::unique_ptr<base::Value>* value = nullptr) {
  MockChrome* chrome = new MockChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));
  WebView* web_view = NULL;
  Status status = chrome->GetWebViewById("1", &web_view);
  if (status.IsError())
    return status;

  std::unique_ptr<base::Value> local_value;
  Timeout timeout;
  return command(&session, web_view, params, value ? value : &local_value,
                 &timeout);
}

Status CallWindowCommand(Command command,
                         StubWebView* web_view,
                         const base::DictionaryValue& params = {},
                         std::unique_ptr<base::Value>* value = nullptr) {
  MockChrome* chrome = new MockChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));

  std::unique_ptr<base::Value> local_value;
  Timeout timeout;
  return command(&session, web_view, params, value ? value : &local_value,
                 &timeout);
}

}  // namespace

TEST(WindowCommandsTest, ExecuteFreeze) {
  Status status = CallWindowCommand(ExecuteFreeze);
  ASSERT_EQ(kOk, status.code());
}

TEST(WindowCommandsTest, ExecuteResume) {
  Status status = CallWindowCommand(ExecuteResume);
  ASSERT_EQ(kOk, status.code());
}

TEST(WindowCommandsTest, ExecuteSendCommandAndGetResult_NoCmd) {
  base::DictionaryValue params;
  params.SetDictionary("params", std::make_unique<base::DictionaryValue>());
  Status status = CallWindowCommand(ExecuteSendCommandAndGetResult, params);
  ASSERT_EQ(kInvalidArgument, status.code());
  ASSERT_NE(status.message().find("command not passed"), std::string::npos);
}

TEST(WindowCommandsTest, ExecuteSendCommandAndGetResult_NoParams) {
  base::DictionaryValue params;
  params.SetString("cmd", "CSS.enable");
  Status status = CallWindowCommand(ExecuteSendCommandAndGetResult, params);
  ASSERT_EQ(kInvalidArgument, status.code());
  ASSERT_NE(status.message().find("params not passed"), std::string::npos);
}

TEST(WindowCommandsTest, ProcessInputActionSequencePointerMouse) {
  Session session("1");
  std::vector<std::unique_ptr<base::DictionaryValue>> action_list;
  std::unique_ptr<base::DictionaryValue> action_sequence(
      new base::DictionaryValue());
  std::unique_ptr<base::ListValue> actions(new base::ListValue());
  std::unique_ptr<base::DictionaryValue> action(new base::DictionaryValue());
  std::unique_ptr<base::DictionaryValue> parameters(
      new base::DictionaryValue());
  parameters->SetString("pointerType", "mouse");
  action->SetString("type", "pointerMove");
  action->SetInteger("x", 30);
  action->SetInteger("y", 60);
  actions->Append(std::move(action));
  action = std::make_unique<base::DictionaryValue>();
  action->SetString("type", "pointerDown");
  action->SetInteger("button", 0);
  actions->Append(std::move(action));
  action = std::make_unique<base::DictionaryValue>();
  action->SetString("type", "pointerUp");
  action->SetInteger("button", 0);
  actions->Append(std::move(action));

  // pointer properties
  action_sequence->SetString("type", "pointer");
  action_sequence->SetString("id", "pointer1");
  action_sequence->SetDictionary("parameters", std::move(parameters));
  action_sequence->SetList("actions", std::move(actions));
  const base::DictionaryValue* input_action_sequence = action_sequence.get();
  Status status =
      ProcessInputActionSequence(&session, input_action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());

  // check resulting action dictionary
  std::string pointer_type;
  std::string source_type;
  std::string id;
  std::string action_type;
  int x, y;
  std::string button;

  ASSERT_EQ(3U, action_list.size());
  const base::DictionaryValue* action1 = action_list[0].get();
  action1->GetString("type", &source_type);
  action1->GetString("pointerType", &pointer_type);
  action1->GetString("id", &id);
  ASSERT_EQ("pointer", source_type);
  ASSERT_EQ("mouse", pointer_type);
  ASSERT_EQ("pointer1", id);
  action1->GetString("subtype", &action_type);
  action1->GetInteger("x", &x);
  action1->GetInteger("y", &y);
  ASSERT_EQ("pointerMove", action_type);
  ASSERT_EQ(30, x);
  ASSERT_EQ(60, y);

  const base::DictionaryValue* action2 = action_list[1].get();
  action2->GetString("type", &source_type);
  action2->GetString("pointerType", &pointer_type);
  action2->GetString("id", &id);
  ASSERT_EQ("pointer", source_type);
  ASSERT_EQ("mouse", pointer_type);
  ASSERT_EQ("pointer1", id);
  action2->GetString("subtype", &action_type);
  action2->GetString("button", &button);
  ASSERT_EQ("pointerDown", action_type);
  ASSERT_EQ("left", button);

  const base::DictionaryValue* action3 = action_list[2].get();
  action3->GetString("type", &source_type);
  action3->GetString("pointerType", &pointer_type);
  action3->GetString("id", &id);
  ASSERT_EQ("pointer", source_type);
  ASSERT_EQ("mouse", pointer_type);
  ASSERT_EQ("pointer1", id);
  action3->GetString("subtype", &action_type);
  action3->GetString("button", &button);
  ASSERT_EQ("pointerUp", action_type);
  ASSERT_EQ("left", button);
}

TEST(WindowCommandsTest, ProcessInputActionSequencePointerTouch) {
  Session session("1");
  std::vector<std::unique_ptr<base::DictionaryValue>> action_list;
  std::unique_ptr<base::DictionaryValue> action_sequence(
      new base::DictionaryValue());
  std::unique_ptr<base::ListValue> actions(new base::ListValue());
  std::unique_ptr<base::DictionaryValue> action(new base::DictionaryValue());
  std::unique_ptr<base::DictionaryValue> parameters(
      new base::DictionaryValue());
  parameters->SetString("pointerType", "touch");
  action->SetString("type", "pointerMove");
  action->SetInteger("x", 30);
  action->SetInteger("y", 60);
  actions->Append(std::move(action));
  action = std::make_unique<base::DictionaryValue>();
  action->SetString("type", "pointerDown");
  actions->Append(std::move(action));
  action = std::make_unique<base::DictionaryValue>();
  action->SetString("type", "pointerUp");
  actions->Append(std::move(action));

  // pointer properties
  action_sequence->SetString("type", "pointer");
  action_sequence->SetString("id", "pointer1");
  action_sequence->SetDictionary("parameters", std::move(parameters));
  action_sequence->SetList("actions", std::move(actions));
  const base::DictionaryValue* input_action_sequence = action_sequence.get();
  Status status =
      ProcessInputActionSequence(&session, input_action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());

  // check resulting action dictionary
  std::string pointer_type;
  std::string source_type;
  std::string id;
  std::string action_type;
  int x, y;

  ASSERT_EQ(3U, action_list.size());
  const base::DictionaryValue* action1 = action_list[0].get();
  action1->GetString("type", &source_type);
  action1->GetString("pointerType", &pointer_type);
  action1->GetString("id", &id);
  ASSERT_EQ("pointer", source_type);
  ASSERT_EQ("touch", pointer_type);
  ASSERT_EQ("pointer1", id);
  action1->GetString("subtype", &action_type);
  action1->GetInteger("x", &x);
  action1->GetInteger("y", &y);
  ASSERT_EQ("pointerMove", action_type);
  ASSERT_EQ(30, x);
  ASSERT_EQ(60, y);

  const base::DictionaryValue* action2 = action_list[1].get();
  action2->GetString("type", &source_type);
  action2->GetString("pointerType", &pointer_type);
  action2->GetString("id", &id);
  ASSERT_EQ("pointer", source_type);
  ASSERT_EQ("touch", pointer_type);
  ASSERT_EQ("pointer1", id);
  action2->GetString("subtype", &action_type);
  ASSERT_EQ("pointerDown", action_type);

  const base::DictionaryValue* action3 = action_list[2].get();
  action3->GetString("type", &source_type);
  action3->GetString("pointerType", &pointer_type);
  action3->GetString("id", &id);
  ASSERT_EQ("pointer", source_type);
  ASSERT_EQ("touch", pointer_type);
  ASSERT_EQ("pointer1", id);
  action3->GetString("subtype", &action_type);
  ASSERT_EQ("pointerUp", action_type);
}

namespace {

class AddCookieWebView : public StubWebView {
 public:
  explicit AddCookieWebView(std::string documentUrl)
      : StubWebView("1"), documentUrl_(documentUrl) {}
  ~AddCookieWebView() override = default;

  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::ListValue& args,
                      std::unique_ptr<base::Value>* result) override {
    if (function.find("document.URL") != std::string::npos) {
      *result = std::make_unique<base::Value>(documentUrl_);
    }
    return Status(kOk);
  }

 private:
  std::string documentUrl_;
};

}  // namespace

TEST(WindowCommandsTest, ExecuteAddCookie_Valid) {
  AddCookieWebView webview = AddCookieWebView("http://chromium.org");
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> cookie_params =
      std::make_unique<base::DictionaryValue>();
  cookie_params->SetString("name", "testcookie");
  cookie_params->SetString("value", "cookievalue");
  cookie_params->SetString("sameSite", "Strict");
  params.SetDictionary("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_NameMissing) {
  AddCookieWebView webview = AddCookieWebView("http://chromium.org");
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> cookie_params =
      std::make_unique<base::DictionaryValue>();
  cookie_params->SetString("value", "cookievalue");
  cookie_params->SetString("sameSite", "invalid");
  params.SetDictionary("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'name'"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_MissingValue) {
  AddCookieWebView webview = AddCookieWebView("http://chromium.org");
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> cookie_params =
      std::make_unique<base::DictionaryValue>();
  cookie_params->SetString("name", "testcookie");
  cookie_params->SetString("sameSite", "Strict");
  params.SetDictionary("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'value'"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_DomainInvalid) {
  AddCookieWebView webview = AddCookieWebView("file://chromium.org");
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> cookie_params =
      std::make_unique<base::DictionaryValue>();
  cookie_params->SetString("name", "testcookie");
  cookie_params->SetString("value", "cookievalue");
  cookie_params->SetString("sameSite", "Strict");
  params.SetDictionary("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kInvalidCookieDomain, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_SameSiteEmpty) {
  AddCookieWebView webview = AddCookieWebView("https://chromium.org");
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> cookie_params =
      std::make_unique<base::DictionaryValue>();
  cookie_params->SetString("name", "testcookie");
  cookie_params->SetString("value", "cookievalue");
  cookie_params->SetString("sameSite", "");
  params.SetDictionary("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_SameSiteNotSet) {
  AddCookieWebView webview = AddCookieWebView("ftp://chromium.org");
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> cookie_params =
      std::make_unique<base::DictionaryValue>();
  cookie_params->SetString("name", "testcookie");
  cookie_params->SetString("value", "cookievalue");
  params.SetDictionary("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
}
