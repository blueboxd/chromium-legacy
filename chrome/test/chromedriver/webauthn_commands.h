// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_WEBAUTHN_COMMANDS_H_
#define CHROME_TEST_CHROMEDRIVER_WEBAUTHN_COMMANDS_H_

#include <memory>

#include "base/callback_forward.h"

namespace base {
class DictionaryValue;
class Value;
}  // namespace base

struct Session;
class Status;
class WebView;

using WebAuthnCommand =
    base::RepeatingCallback<Status(WebView* web_view,
                                   const base::Value& params,
                                   std::unique_ptr<base::Value>* value)>;

// Executes a WebAuthn command.
Status ExecuteWebAuthnCommand(const WebAuthnCommand& command,
                              Session* session,
                              const base::DictionaryValue& params,
                              std::unique_ptr<base::Value>* value);

// Add a virtual authenticator.
Status ExecuteAddVirtualAuthenticator(WebView* web_view,
                                      const base::Value& params,
                                      std::unique_ptr<base::Value>* value);

// Remove a virtual authenticator.
Status ExecuteRemoveVirtualAuthenticator(WebView* web_view,
                                         const base::Value& params,
                                         std::unique_ptr<base::Value>* value);

#endif  // CHROME_TEST_CHROMEDRIVER_WEBAUTHN_COMMANDS_H_
