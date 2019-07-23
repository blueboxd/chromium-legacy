// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/webauthn_commands.h"

#include <utility>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/session.h"

Status ExecuteWebAuthnCommand(const WebAuthnCommand& command,
                              Session* session,
                              const base::DictionaryValue& params,
                              std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  status = web_view->ConnectIfNecessary();
  if (status.IsError())
    return status;

  status = web_view->SendCommand("WebAuthn.enable", base::DictionaryValue());
  if (status.IsError())
    return status;

  return command.Run(web_view, params, value);
}

Status ExecuteAddVirtualAuthenticator(WebView* web_view,
                                      const base::Value& params,
                                      std::unique_ptr<base::Value>* value) {
  base::DictionaryValue options;
  const base::flat_map<const char*, const char*> mapping = {
      {"options.protocol", "protocol"},
      {"options.transport", "transport"},
      {"options.hasResidentKey", "hasResidentKey"},
      {"options.hasUserVerification", "hasUserVerification"},
      {"options.automaticPresenceSimulation", "isUserVerified"},
  };
  for (const std::pair<const char*, const char*>& pair : mapping) {
    const base::Value* value = params.FindKey(pair.second);
    if (value)
      options.SetPath(pair.first, value->Clone());
  }

  return web_view->SendCommandAndGetResult("WebAuthn.addVirtualAuthenticator",
                                           options, value);
}

Status ExecuteRemoveVirtualAuthenticator(WebView* web_view,
                                         const base::Value& params,
                                         std::unique_ptr<base::Value>* value) {
  base::DictionaryValue options;
  const base::Value* authenticatorId = params.FindKey("authenticatorId");
  if (authenticatorId)
    options.SetKey("authenticatorId", authenticatorId->Clone());

  return web_view->SendCommandAndGetResult(
      "WebAuthn.removeVirtualAuthenticator", options, value);
}
