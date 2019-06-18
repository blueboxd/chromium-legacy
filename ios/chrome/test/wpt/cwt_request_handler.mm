// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/wpt/cwt_request_handler.h"

#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/sys_string_conversions.h"
#include "components/version_info/version_info.h"
#import "ios/chrome/test/wpt/cwt_webdriver_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/http/http_status_code.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

GREY_STUB_CLASS_IN_APP_BACKGROUND_QUEUE(CWTWebDriverAppInterface)

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace {

const NSTimeInterval kDefaultScriptTimeout = 30;
const NSTimeInterval kDefaultPageLoadTimeout = 300;

// WebDriver commands.
const char kWebDriverSessionCommand[] = "session";
const char kWebDriverNavigationCommand[] = "url";
const char kWebDriverTimeoutsCommand[] = "timeouts";
const char kWebDriverWindowCommand[] = "window";
const char kWebDriverWindowHandlesCommand[] = "handles";

// WebDriver error codes.
const char kWebDriverInvalidArgumentError[] = "invalid argument";
const char kWebDriverInvalidSessionError[] = "invalid session id";
const char kWebDriverSessionCreationError[] = "session not created";
const char kWebDriverTimeoutError[] = "timeout";
const char kWebDriverNoSuchWindowError[] = "no such window";
const char kWebDriverUnknownCommandError[] = "unknown command";

// WebDriver error messages. The content of each message is implementation-
// defined, not prescribed the by the spec.
const char kWebDriverMissingRequestMessage[] = "Missing request body";
const char kWebDriverMissingURLMessage[] = "No url argument";
const char kWebDriverNoActiveSessionMessage[] = "No currently active session";
const char kWebDriverPageLoadTimeoutMessage[] = "Page load timed out";
const char kWebDriverSessionAlreadyExistsMessage[] = "A session already exists";
const char kWebDriverUnknownCommandMessage[] = "No such command";
const char kWebDriverInvalidTimeoutMessage[] =
    "Timeouts must be non-negative integers";
const char kWebDriverNoOpenWindowMessage[] = "No currently open window";
const char kWebDriverMissingWindowHandleMessage[] = "No handle argument";
const char kWebDriverNoMatchingWindowMessage[] =
    "No window with the given handle";

// WebDriver request field names. These are fields that are contained within
// the body of a POST request.
const char kWebDriverURLRequestField[] = "url";
const char kWebDriverScriptTimeoutRequestField[] = "script";
const char kWebDriverPageLoadTimeoutRequestField[] = "pageLoad";
const char kWebDriverWindowHandleRequestField[] = "handle";

// WebDriver response field name. This is the top-level field in the JSON object
// contained in a response.
const char kWebDriverValueResponseField[] = "value";

// WebDriver value field names. These fields are contained within the 'value'
// field in a WebDriver reponse. Each response value has zero or more of these
// fields.
const char kWebDriverCapabilitiesValueField[] = "capabilities";
const char kWebDriverErrorCodeValueField[] = "error";
const char kWebDriverErrorMessageValueField[] = "message";
const char kWebDriverSessionIdValueField[] = "sessionId";

// Field names for the "capabilities" struct that's included in the response
// when creating a session.
const char kCapabilitiesBrowserNameField[] = "browserName";
const char kCapabilitiesBrowserVersionField[] = "browserVersion";
const char kCapabilitiesPlatformNameField[] = "platformName";
const char kCapabilitiesPageLoadStrategyField[] = "pageLoadStrategy";
const char kCapabilitiesProxyField[] = "proxy";
const char kCapabilitiesScriptTimeoutField[] = "timeouts.script";
const char kCapabilitiesPageLoadTimeoutField[] = "timeouts.pageLoad";
const char kCapabilitiesImplicitTimeoutField[] = "timeouts.implicit";
const char kCapabilitiesCanResizeWindowsField[] = "setWindowRect";

// Field values for the "capabilities" struct that's included in the response
// when creating a session.
const char kCapabilitiesBrowserName[] = "chrome_ios";
const char kCapabilitiesPlatformName[] = "iOS";
const char kCapabilitiesPageLoadStrategy[] = "normal";

base::Value CreateErrorValue(const std::string& error,
                             const std::string& message) {
  base::Value error_value(base::Value::Type::DICTIONARY);
  error_value.SetStringKey(kWebDriverErrorCodeValueField, error);
  error_value.SetStringKey(kWebDriverErrorMessageValueField, message);
  return error_value;
}

bool IsErrorValue(const base::Value& value) {
  return value.is_dict() && value.FindKey(kWebDriverErrorCodeValueField);
}

}  // namespace

CWTRequestHandler::CWTRequestHandler(ProceduralBlock session_completion_handler)
    : session_completion_handler_(session_completion_handler),
      script_timeout_(kDefaultScriptTimeout),
      page_load_timeout_(kDefaultPageLoadTimeout) {}

base::Optional<base::Value> CWTRequestHandler::ProcessCommand(
    const std::string& command,
    net::test_server::HttpMethod http_method,
    const std::string& request_content) {
  if (http_method == net::test_server::METHOD_GET) {
    if (session_id_.empty()) {
      return CreateErrorValue(kWebDriverInvalidSessionError,
                              kWebDriverNoActiveSessionMessage);
    }

    if (command == kWebDriverWindowCommand)
      return GetCurrentTabId();

    if (command == kWebDriverWindowHandlesCommand)
      return GetAllTabIds();

    return base::nullopt;
  }

  if (http_method == net::test_server::METHOD_POST) {
    base::Optional<base::Value> content =
        base::JSONReader::Read(request_content);
    if (!content || !content->is_dict()) {
      return CreateErrorValue(kWebDriverInvalidArgumentError,
                              kWebDriverMissingRequestMessage);
    }

    if (command == kWebDriverSessionCommand)
      return InitializeSession();

    if (session_id_.empty()) {
      return CreateErrorValue(kWebDriverInvalidSessionError,
                              kWebDriverNoActiveSessionMessage);
    }

    if (command == kWebDriverNavigationCommand)
      return NavigateToUrl(content->FindKey(kWebDriverURLRequestField));

    if (command == kWebDriverTimeoutsCommand)
      return SetTimeouts(*content);

    if (command == kWebDriverWindowCommand) {
      return SwitchToTabWithId(
          content->FindKey(kWebDriverWindowHandleRequestField));
    }

    return base::nullopt;
  }

  if (http_method == net::test_server::METHOD_DELETE) {
    if (session_id_.empty()) {
      return CreateErrorValue(kWebDriverInvalidSessionError,
                              kWebDriverNoActiveSessionMessage);
    }

    if (command == session_id_)
      return CloseSession();

    if (command == kWebDriverWindowCommand)
      return CloseCurrentTab();

    return base::nullopt;
  }

  return base::nullopt;
}

std::unique_ptr<net::test_server::HttpResponse>
CWTRequestHandler::HandleRequest(const net::test_server::HttpRequest& request) {
  std::string command = request.GetURL().ExtractFileName();
  base::Optional<base::Value> result =
      ProcessCommand(command, request.method, request.content);

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("application/json; charset=utf-8");
  response->AddCustomHeader("Cache-Control", "no-cache");
  if (!result) {
    response->set_code(net::HTTP_NOT_FOUND);
    result = CreateErrorValue(kWebDriverUnknownCommandError,
                              kWebDriverUnknownCommandMessage);
  } else if (IsErrorValue(*result)) {
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
  } else {
    response->set_code(net::HTTP_OK);
  }

  base::Value response_content(base::Value::Type::DICTIONARY);
  response_content.SetKey(kWebDriverValueResponseField, std::move(*result));
  std::string response_content_string;
  base::JSONWriter::Write(response_content, &response_content_string);
  response->set_content(response_content_string);

  return std::move(response);
}

base::Value CWTRequestHandler::InitializeSession() {
  if (!session_id_.empty()) {
    return CreateErrorValue(kWebDriverSessionCreationError,
                            kWebDriverSessionAlreadyExistsMessage);
  }
  base::Value result(base::Value::Type::DICTIONARY);
  session_id_ = base::GenerateGUID();
  result.SetStringKey(kWebDriverSessionIdValueField, session_id_);

  base::Value capabilities(base::Value::Type::DICTIONARY);
  capabilities.SetStringKey(kCapabilitiesBrowserNameField,
                            kCapabilitiesBrowserName);
  capabilities.SetStringKey(kCapabilitiesBrowserVersionField,
                            version_info::GetVersionNumber());
  capabilities.SetStringKey(kCapabilitiesPlatformNameField,
                            kCapabilitiesPlatformName);
  capabilities.SetStringKey(kCapabilitiesPageLoadStrategyField,
                            kCapabilitiesPageLoadStrategy);
  capabilities.SetKey(kCapabilitiesProxyField,
                      base::Value(base::Value::Type::DICTIONARY));
  capabilities.SetIntPath(kCapabilitiesScriptTimeoutField,
                          script_timeout_ * 1000);
  capabilities.SetIntPath(kCapabilitiesPageLoadTimeoutField,
                          page_load_timeout_ * 1000);
  capabilities.SetIntPath(kCapabilitiesImplicitTimeoutField, 0);
  capabilities.SetKey(kCapabilitiesCanResizeWindowsField, base::Value(false));

  result.SetKey(kWebDriverCapabilitiesValueField, std::move(capabilities));
  return result;
}

base::Value CWTRequestHandler::CloseSession() {
  session_id_.clear();
  session_completion_handler_();
  return base::Value(base::Value::Type::NONE);
}

base::Value CWTRequestHandler::NavigateToUrl(const base::Value* url) {
  if (!url || !url->is_string()) {
    return CreateErrorValue(kWebDriverInvalidArgumentError,
                            kWebDriverMissingURLMessage);
  }

  NSError* error = [CWTWebDriverAppInterface
               loadURL:base::SysUTF8ToNSString(url->GetString())
      timeoutInSeconds:page_load_timeout_];
  if (!error)
    return base::Value(base::Value::Type::NONE);

  return CreateErrorValue(kWebDriverTimeoutError,
                          kWebDriverPageLoadTimeoutMessage);
}

base::Value CWTRequestHandler::SetTimeouts(const base::Value& timeouts) {
  for (const auto& timeout : timeouts.DictItems()) {
    if (!timeout.second.is_int() || timeout.second.GetInt() < 0) {
      return CreateErrorValue(kWebDriverInvalidArgumentError,
                              kWebDriverInvalidTimeoutMessage);
    }

    int timeout_in_milliseconds = timeout.second.GetInt();
    NSTimeInterval timeout_in_seconds =
        static_cast<double>(timeout_in_milliseconds) / 1000;

    // Only script and page load timeouts are supported in CWTChromeDriver.
    // Other values are ignored.
    if (timeout.first == kWebDriverScriptTimeoutRequestField)
      script_timeout_ = timeout_in_seconds;
    else if (timeout.first == kWebDriverPageLoadTimeoutRequestField)
      page_load_timeout_ = timeout_in_seconds;
  }
  return base::Value(base::Value::Type::NONE);
}

base::Value CWTRequestHandler::GetCurrentTabId() {
  NSString* tab_id = [CWTWebDriverAppInterface getCurrentTabID];
  if (!tab_id) {
    return CreateErrorValue(kWebDriverNoSuchWindowError,
                            kWebDriverNoOpenWindowMessage);
  }

  return base::Value(base::SysNSStringToUTF8(tab_id));
}

base::Value CWTRequestHandler::GetAllTabIds() {
  base::Value id_list(base::Value::Type::LIST);
  NSArray* tab_ids = [CWTWebDriverAppInterface getTabIDs];
  for (NSString* tab_id in tab_ids) {
    id_list.GetList().push_back(base::Value(base::SysNSStringToUTF8(tab_id)));
  }
  return id_list;
}

base::Value CWTRequestHandler::SwitchToTabWithId(const base::Value* id) {
  if (!id || !id->is_string()) {
    return CreateErrorValue(kWebDriverInvalidArgumentError,
                            kWebDriverMissingWindowHandleMessage);
  }

  NSError* error = [CWTWebDriverAppInterface
      switchToTabWithID:base::SysUTF8ToNSString(id->GetString())];

  if (!error)
    return base::Value(base::Value::Type::NONE);

  return CreateErrorValue(kWebDriverNoSuchWindowError,
                          kWebDriverNoMatchingWindowMessage);
}

base::Value CWTRequestHandler::CloseCurrentTab() {
  NSError* error = [CWTWebDriverAppInterface closeCurrentTab];

  if (error) {
    return CreateErrorValue(kWebDriverNoSuchWindowError,
                            kWebDriverNoOpenWindowMessage);
  }

  return GetAllTabIds();
}
