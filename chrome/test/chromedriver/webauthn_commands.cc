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

namespace {

// Creates a base::DictionaryValue by cloning the parameters specified by
// |mapping| from |params|.
base::DictionaryValue MapParams(
    const base::flat_map<const char*, const char*>& mapping,
    const base::Value& params) {
  base::DictionaryValue options;
  for (const std::pair<const char*, const char*>& pair : mapping) {
    const base::Value* value = params.FindKey(pair.second);
    if (value)
      options.SetPath(pair.first, value->Clone());
  }
  return options;
}

}  // namespace

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
  return web_view->SendCommandAndGetResult(
      "WebAuthn.addVirtualAuthenticator",
      MapParams(
          {
              {"options.protocol", "protocol"},
              {"options.transport", "transport"},
              {"options.hasResidentKey", "hasResidentKey"},
              {"options.hasUserVerification", "hasUserVerification"},
              {"options.automaticPresenceSimulation", "isUserVerified"},
          },
          params),
      value);
}

Status ExecuteRemoveVirtualAuthenticator(WebView* web_view,
                                         const base::Value& params,
                                         std::unique_ptr<base::Value>* value) {
  return web_view->SendCommandAndGetResult(
      "WebAuthn.removeVirtualAuthenticator",
      MapParams({{"authenticatorId", "authenticatorId"}}, params), value);
}

Status ExecuteAddCredential(WebView* web_view,
                            const base::Value& params,
                            std::unique_ptr<base::Value>* value) {
  return web_view->SendCommandAndGetResult(
      "WebAuthn.addCredential",
      MapParams(
          {
              {"authenticatorId", "authenticatorId"},
              {"credential.credentialId", "credentialId"},
              {"credential.isResidentCredential", "isResidentCredential"},
              {"credential.rpId", "rpId"},
              {"credential.privateKey", "privateKey"},
              {"credential.userHandle", "userHandle"},
              {"credential.signCount", "signCount"},
          },
          params),
      value);
}
