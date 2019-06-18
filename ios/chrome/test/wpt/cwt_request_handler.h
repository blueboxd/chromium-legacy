// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_WPT_CWT_REQUEST_HANDLER_H_
#define IOS_CHROME_TEST_WPT_CWT_REQUEST_HANDLER_H_

#import <Foundation/Foundation.h>
#include <string>

#include "base/ios/block_types.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/values.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

// Implements a subset of the WebDriver protocol, for running Web Platform
// Tests. This not intended to be a general-purpose WebDriver implementation.
// Each CWTRequestHandler supports only a single session. Additional
// requests to create a session while one is already active will return an
// error response.
//
// See https://w3c.github.io/webdriver/ for the complete WebDriver protocol
// specification. In addition to only implementing a subset of this protocol,
// CWTRequestHandler only performs minimal error-checking for the sake
// of making clients easier to debug, and otherwise assumes that the client
// is submitting well-formed requests, and that all requests are coming from
// a single client.
//
// For example, to create a session, load a URL, and then close the session, the
// following sequence of requests can be used:
// 1) POST /session
// 2) POST /url (with a content body that is a JSON dictionary whose 'url'
//    property's value is the URL to navigate to)
// 3) DELETE /session/{session id}
class CWTRequestHandler {
 public:
  // |session_completion_handler| is a block to be called when a session is
  // closed.
  CWTRequestHandler(ProceduralBlock sesssion_completion_handler);

  // Creates responses for HTTP requests according to the WebDriver protocol.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

 private:
  // Creates a new session, if no session has already been created. Otherwise,
  // return an error.
  base::Value InitializeSession();

  // Terminates the current session.
  base::Value CloseSession();

  // Navigates the current tab to the given URL, and wait for the page load to
  // complete.
  base::Value NavigateToUrl(const base::Value* url);

  // Sets timeouts used when performing browser operations.
  base::Value SetTimeouts(const base::Value& timeouts);

  // Gets the id of the current tab. Returns an error value if no tab is open.
  base::Value GetCurrentTabId();

  // Gets the ids of all open tabs.
  base::Value GetAllTabIds();

  // Switches to the tab with the given id. Returns an error value if no such
  // tab exists.
  base::Value SwitchToTabWithId(const base::Value* id);

  // Closes the current tab. Returns an error value if no tab is open.
  // Otherwise, returns the ids of the remaining tabs.
  base::Value CloseCurrentTab();

  // Processes the given command, HTTP method, and request content. Returns the
  // result of processing the command, or nullopt_t if the command is unknown.
  base::Optional<base::Value> ProcessCommand(
      const std::string& command,
      net::test_server::HttpMethod http_method,
      const std::string& request_content);

  // Block that gets called when a session is terminated.
  ProceduralBlock session_completion_handler_;

  // A randomly-generated identifier created by InitializeSession().
  std::string session_id_;

  // Timeouts used when performing browser operations.
  NSTimeInterval script_timeout_;
  NSTimeInterval page_load_timeout_;

  DISALLOW_COPY_AND_ASSIGN(CWTRequestHandler);
};

#endif  // IOS_CHROME_TEST_WPT_CWT_REQUEST_HANDLER_H_
