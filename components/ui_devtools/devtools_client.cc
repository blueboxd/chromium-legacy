// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/devtools_client.h"

#include "components/ui_devtools/devtools_server.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace ui_devtools {

UiDevToolsClient::UiDevToolsClient(const std::string& name,
                                   UiDevToolsServer* server)
    : name_(name),
      connection_id_(kNotConnected),
      dispatcher_(this),
      server_(server) {
  DCHECK(server_);
}

UiDevToolsClient::~UiDevToolsClient() {}

void UiDevToolsClient::AddAgent(std::unique_ptr<UiDevToolsAgent> agent) {
  agent->Init(&dispatcher_);
  agents_.push_back(std::move(agent));
}

void UiDevToolsClient::Disconnect() {
  connection_id_ = kNotConnected;
  DisableAllAgents();
}

void UiDevToolsClient::Dispatch(const std::string& json) {
  std::vector<uint8_t> cbor;
  crdtp::Status status =
      crdtp::json::ConvertJSONToCBOR(crdtp::SpanFrom(json), &cbor);
  LOG_IF(ERROR, !status.ok()) << status.ToASCIIString();

  int call_id;
  std::string method;
  std::unique_ptr<protocol::Value> protocolCommand =
      protocol::Value::parseBinary(cbor.data(), cbor.size());
  if (dispatcher_.parseCommand(protocolCommand.get(), &call_id, &method)) {
    dispatcher_.dispatch(call_id, method, std::move(protocolCommand),
                         crdtp::SpanFrom(cbor));
  }
}

bool UiDevToolsClient::connected() const {
  return connection_id_ != kNotConnected;
}

void UiDevToolsClient::set_connection_id(int connection_id) {
  connection_id_ = connection_id;
}

const std::string& UiDevToolsClient::name() const {
  return name_;
}

void UiDevToolsClient::DisableAllAgents() {
  for (std::unique_ptr<UiDevToolsAgent>& agent : agents_)
    agent->Disable();
}

void UiDevToolsClient::MaybeSendProtocolResponseOrNotification(
    std::unique_ptr<protocol::Serializable> message) {
  if (!connected())
    return;

  std::string json;
  crdtp::Status status = crdtp::json::ConvertCBORToJSON(
      crdtp::SpanFrom(message->Serialize()), &json);
  LOG_IF(ERROR, !status.ok()) << status.ToASCIIString();
  server_->SendOverWebSocket(connection_id_, base::StringPiece(json));
}

void UiDevToolsClient::sendProtocolResponse(
    int callId,
    std::unique_ptr<protocol::Serializable> message) {
  MaybeSendProtocolResponseOrNotification(std::move(message));
}

void UiDevToolsClient::sendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  MaybeSendProtocolResponseOrNotification(std::move(message));
}

void UiDevToolsClient::flushProtocolNotifications() {
  NOTIMPLEMENTED();
}

void UiDevToolsClient::fallThrough(int call_id,
                                   const std::string& method,
                                   crdtp::span<uint8_t> message) {
  NOTIMPLEMENTED();
}

}  // namespace ui_devtools
