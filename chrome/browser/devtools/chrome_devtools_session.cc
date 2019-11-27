// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_session.h"

#include <memory>
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/devtools/protocol/browser_handler.h"
#include "chrome/browser/devtools/protocol/cast_handler.h"
#include "chrome/browser/devtools/protocol/page_handler.h"
#include "chrome/browser/devtools/protocol/security_handler.h"
#include "chrome/browser/devtools/protocol/target_handler.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "third_party/inspector_protocol/crdtp/json.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/devtools/protocol/window_manager_handler.h"
#endif

ChromeDevToolsSession::ChromeDevToolsSession(
    content::DevToolsAgentHost* agent_host,
    content::DevToolsAgentHostClient* client)
    : agent_host_(agent_host), client_(client), dispatcher_(this) {
  if (agent_host->GetWebContents() &&
      agent_host->GetType() == content::DevToolsAgentHost::kTypePage) {
    page_handler_ = std::make_unique<PageHandler>(agent_host->GetWebContents(),
                                                  &dispatcher_);
    security_handler_ = std::make_unique<SecurityHandler>(
        agent_host->GetWebContents(), &dispatcher_);
    if (client->MayAttachToBrowser()) {
      cast_handler_ = std::make_unique<CastHandler>(
          agent_host->GetWebContents(), &dispatcher_);
    }
  }
  target_handler_ = std::make_unique<TargetHandler>(&dispatcher_);
  if (client->MayAttachToBrowser()) {
    browser_handler_ =
        std::make_unique<BrowserHandler>(&dispatcher_, agent_host->GetId());
  }
#if defined(OS_CHROMEOS)
  window_manager_handler_ =
      std::make_unique<WindowManagerHandler>(&dispatcher_);
#endif
}

ChromeDevToolsSession::~ChromeDevToolsSession() = default;

void ChromeDevToolsSession::HandleCommand(
    const std::string& method,
    const std::string& message,
    content::DevToolsManagerDelegate::NotHandledCallback callback) {
  if (!dispatcher_.canDispatch(method)) {
    std::move(callback).Run(message);
    return;
  }

  int call_id;
  std::string unused;
  std::unique_ptr<protocol::DictionaryValue> value =
      protocol::DictionaryValue::cast(
          protocol::StringUtil::parseMessage(message, /*binary=*/true));
  if (!dispatcher_.parseCommand(value.get(), &call_id, &unused))
    return;
  pending_commands_[call_id] = std::move(callback);
  dispatcher_.dispatch(call_id, method, std::move(value), message);
}

// The following methods handle responses or notifications coming from
// the browser to the client.
static void SendProtocolResponseOrNotification(
    content::DevToolsAgentHostClient* client,
    content::DevToolsAgentHost* agent_host,
    std::unique_ptr<protocol::Serializable> message) {
  std::vector<uint8_t> cbor = std::move(*message).TakeSerialized();
  if (client->UsesBinaryProtocol()) {
    client->DispatchProtocolMessage(agent_host,
                                    std::string(cbor.begin(), cbor.end()));
    return;
  }
  std::string json;
  crdtp::Status status =
      crdtp::json::ConvertCBORToJSON(crdtp::SpanFrom(cbor), &json);
  LOG_IF(ERROR, !status.ok()) << status.ToASCIIString();
  client->DispatchProtocolMessage(agent_host, json);
}

void ChromeDevToolsSession::sendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  pending_commands_.erase(call_id);
  SendProtocolResponseOrNotification(client_, agent_host_, std::move(message));
}

void ChromeDevToolsSession::sendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  SendProtocolResponseOrNotification(client_, agent_host_, std::move(message));
}

void ChromeDevToolsSession::flushProtocolNotifications() {}

void ChromeDevToolsSession::fallThrough(int call_id,
                                        const std::string& method,
                                        const std::string& message) {
  auto callback = std::move(pending_commands_[call_id]);
  pending_commands_.erase(call_id);
  std::move(callback).Run(message);
}
