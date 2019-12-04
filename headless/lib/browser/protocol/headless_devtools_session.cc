// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/protocol/headless_devtools_session.h"

#include "base/command_line.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "headless/lib/browser/protocol/browser_handler.h"
#include "headless/lib/browser/protocol/headless_handler.h"
#include "headless/lib/browser/protocol/page_handler.h"
#include "headless/lib/browser/protocol/target_handler.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace headless {
namespace protocol {
HeadlessDevToolsSession::HeadlessDevToolsSession(
    base::WeakPtr<HeadlessBrowserImpl> browser,
    content::DevToolsAgentHost* agent_host,
    content::DevToolsAgentHostClient* client)
    : browser_(browser),
      agent_host_(agent_host),
      client_(client),
      dispatcher_(this) {
  if (agent_host->GetWebContents() &&
      agent_host->GetType() == content::DevToolsAgentHost::kTypePage) {
    AddHandler(std::make_unique<HeadlessHandler>(browser_.get(),
                                                 agent_host->GetWebContents()));
    AddHandler(std::make_unique<PageHandler>(agent_host,
                                             agent_host->GetWebContents()));
  }
  if (client->MayAttachToBrowser()) {
    AddHandler(
        std::make_unique<BrowserHandler>(browser_.get(), agent_host->GetId()));
  }
  AddHandler(std::make_unique<TargetHandler>(browser_.get()));
}

HeadlessDevToolsSession::~HeadlessDevToolsSession() {
  for (auto& handler : handlers_)
    handler->Disable();
}

void HeadlessDevToolsSession::HandleCommand(
    const std::string& method,
    const std::string& message,
    content::DevToolsManagerDelegate::NotHandledCallback callback) {
  if (!browser_ || !dispatcher_.canDispatch(method)) {
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

void HeadlessDevToolsSession::AddHandler(
    std::unique_ptr<protocol::DomainHandler> handler) {
  handler->Wire(&dispatcher_);
  handlers_.push_back(std::move(handler));
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

void HeadlessDevToolsSession::sendProtocolResponse(
    int call_id,
    std::unique_ptr<Serializable> message) {
  pending_commands_.erase(call_id);
  SendProtocolResponseOrNotification(client_, agent_host_, std::move(message));
}

void HeadlessDevToolsSession::sendProtocolNotification(
    std::unique_ptr<Serializable> message) {
  SendProtocolResponseOrNotification(client_, agent_host_, std::move(message));
}

void HeadlessDevToolsSession::flushProtocolNotifications() {}

void HeadlessDevToolsSession::fallThrough(int call_id,
                                          const std::string& method,
                                          const std::string& message) {
  auto callback = std::move(pending_commands_[call_id]);
  pending_commands_.erase(call_id);
  std::move(callback).Run(message);
}
}  // namespace protocol
}  // namespace headless
