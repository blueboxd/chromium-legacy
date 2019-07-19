// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_session.h"

#include <vector>

#include "base/bind.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/devtools_protocol_encoding.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "third_party/inspector_protocol/encoding/encoding.h"

namespace content {
namespace {
using ::inspector_protocol_encoding::span;
using ::inspector_protocol_encoding::SpanFrom;
using ::inspector_protocol_encoding::cbor::AppendString8EntryToCBORMap;
using ::inspector_protocol_encoding::cbor::IsCBORMessage;
using IPEStatus = ::inspector_protocol_encoding::Status;

bool ShouldSendOnIO(const std::string& method) {
  // Keep in sync with WebDevToolsAgent::ShouldInterruptForMethod.
  // TODO(einbinder): find a way to share this.
  return method == "Debugger.pause" || method == "Debugger.setBreakpoint" ||
         method == "Debugger.setBreakpointByUrl" ||
         method == "Debugger.removeBreakpoint" ||
         method == "Debugger.setBreakpointsActive" ||
         method == "Debugger.getStackTrace" ||
         method == "Performance.getMetrics" || method == "Page.crash" ||
         method == "Runtime.terminateExecution" ||
         method == "Emulation.setScriptExecutionDisabled";
}

static const char kMethod[] = "method";
static const char kResumeMethod[] = "Runtime.runIfWaitingForDebugger";
static const char kSessionId[] = "sessionId";
}  // namespace

DevToolsSession::DevToolsSession(DevToolsAgentHostClient* client)
    : binding_(this),
      client_(client),
      dispatcher_(new protocol::UberDispatcher(this)) {}

DevToolsSession::~DevToolsSession() {
  if (proxy_delegate_)
    proxy_delegate_->Detach(this);
  // It is Ok for session to be deleted without the dispose -
  // it can be kicked out by an extension connect / disconnect.
  if (dispatcher_)
    Dispose();
}

void DevToolsSession::SetAgentHost(DevToolsAgentHostImpl* agent_host) {
  DCHECK(!agent_host_);
  agent_host_ = agent_host;
}

void DevToolsSession::SetRuntimeResumeCallback(
    base::OnceClosure runtime_resume) {
  runtime_resume_ = std::move(runtime_resume);
}

void DevToolsSession::Dispose() {
  dispatcher_.reset();
  for (auto& pair : handlers_)
    pair.second->Disable();
  handlers_.clear();
}

DevToolsSession* DevToolsSession::GetRootSession() {
  return root_session_ ? root_session_ : this;
}

void DevToolsSession::AddHandler(
    std::unique_ptr<protocol::DevToolsDomainHandler> handler) {
  DCHECK(agent_host_);
  handler->Wire(dispatcher_.get());
  handlers_[handler->name()] = std::move(handler);
}

void DevToolsSession::SetBrowserOnly(bool browser_only) {
  browser_only_ = browser_only;
}

void DevToolsSession::TurnIntoExternalProxy(
    DevToolsExternalAgentProxyDelegate* proxy_delegate) {
  proxy_delegate_ = proxy_delegate;
  proxy_delegate_->Attach(this);
}

void DevToolsSession::AttachToAgent(blink::mojom::DevToolsAgent* agent) {
  DCHECK(agent_host_);
  if (!agent) {
    binding_.Close();
    session_ptr_.reset();
    io_session_ptr_.reset();
    return;
  }

  blink::mojom::DevToolsSessionHostAssociatedPtrInfo host_ptr_info;
  binding_.Bind(mojo::MakeRequest(&host_ptr_info));
  agent->AttachDevToolsSession(
      std::move(host_ptr_info), mojo::MakeRequest(&session_ptr_),
      mojo::MakeRequest(&io_session_ptr_), session_state_cookie_.Clone(),
      client_->UsesBinaryProtocol());
  session_ptr_.set_connection_error_handler(base::BindOnce(
      &DevToolsSession::MojoConnectionDestroyed, base::Unretained(this)));

  if (!suspended_sending_messages_to_agent_) {
    for (const auto& pair : waiting_for_response_messages_) {
      int call_id = pair.first;
      const WaitingMessage& message = pair.second;
      DispatchProtocolMessageToAgent(call_id, message.method, message.message);
    }
  } else {
    std::vector<SuspendedMessage> temp;
    for (const auto& pair : waiting_for_response_messages_)
      temp.push_back({pair.first, pair.second.method, pair.second.message});
    suspended_messages_.insert(suspended_messages_.begin(), temp.begin(),
                               temp.end());
    waiting_for_response_messages_.clear();
  }

  // Set cookie to an empty struct to reattach next time instead of attaching.
  if (!session_state_cookie_)
    session_state_cookie_ = blink::mojom::DevToolsSessionState::New();
}

void DevToolsSession::MojoConnectionDestroyed() {
  binding_.Close();
  session_ptr_.reset();
  io_session_ptr_.reset();
}

// The client of the devtools session will call this method to send a message
// to handlers / agents that the session is connected with.
bool DevToolsSession::DispatchProtocolMessage(const std::string& message) {
  // If the session is in proxy mode, then |message| will be sent to
  // an external session, so it needs to be sent as JSON.
  // TODO(dgozman): revisit the proxy delegate.
  if (proxy_delegate_) {
    if (client_->UsesBinaryProtocol()) {
      DCHECK(IsCBORMessage(SpanFrom(message)));
      std::string json;
      IPEStatus status = ConvertCBORToJSON(SpanFrom(message), &json);
      LOG_IF(ERROR, !status.ok()) << status.ToASCIIString();
      proxy_delegate_->SendMessageToBackend(this, json);
      return true;
    }
    proxy_delegate_->SendMessageToBackend(this, message);
    return true;
  }
  std::string converted_cbor_message;
  const std::string* message_to_send = &message;
  if (client_->UsesBinaryProtocol()) {
    // If the client uses the binary protocol, then |message| is already
    // CBOR (it comes from the client).
    DCHECK(IsCBORMessage(SpanFrom(message)));
  } else {
    IPEStatus status =
        ConvertJSONToCBOR(SpanFrom(message), &converted_cbor_message);
    LOG_IF(ERROR, !status.ok()) << status.ToASCIIString();
    message_to_send = &converted_cbor_message;
  }
  std::unique_ptr<protocol::DictionaryValue> value =
      protocol::DictionaryValue::cast(
          protocol::StringUtil::parseMessage(*message_to_send, true));

  std::string session_id;
  if (!value || !value->getString(kSessionId, &session_id))
    return DispatchProtocolMessageInternal(*message_to_send, std::move(value));

  auto it = child_sessions_.find(session_id);
  if (it == child_sessions_.end())
    return false;
  DevToolsSession* session = it->second;
  DCHECK(!session->proxy_delegate_);
  return session->DispatchProtocolMessageInternal(*message_to_send,
                                                  std::move(value));
}

bool DevToolsSession::DispatchProtocolMessageInternal(
    const std::string& message,
    std::unique_ptr<protocol::DictionaryValue> value) {
  std::string method;
  bool has_method = value && value->getString(kMethod, &method);
  if (!runtime_resume_.is_null() && has_method && method == kResumeMethod)
    std::move(runtime_resume_).Run();

  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (delegate && has_method) {
    delegate->HandleCommand(
        agent_host_, client_, method, message,
        base::BindOnce(&DevToolsSession::HandleCommand,
                       weak_factory_.GetWeakPtr(), std::move(value)));
  } else {
    HandleCommand(std::move(value), message);
  }
  return true;
}

void DevToolsSession::HandleCommand(
    std::unique_ptr<protocol::DictionaryValue> value,
    const std::string& message) {
  int call_id;
  std::string method;
  if (!dispatcher_->parseCommand(value.get(), &call_id, &method))
    return;
  if (browser_only_ || dispatcher_->canDispatch(method)) {
    dispatcher_->dispatch(call_id, method, std::move(value), message);
  } else {
    fallThrough(call_id, method, message);
  }
}

void DevToolsSession::fallThrough(int call_id,
                                  const std::string& method,
                                  const std::string& message) {
  // In browser-only mode, we should've handled everything in dispatcher.
  DCHECK(!browser_only_);

  if (suspended_sending_messages_to_agent_) {
    suspended_messages_.push_back({call_id, method, message});
    return;
  }

  DispatchProtocolMessageToAgent(call_id, method, message);
  waiting_for_response_messages_[call_id] = {method, message};
}

void DevToolsSession::DispatchProtocolMessageToAgent(
    int call_id,
    const std::string& method,
    const std::string& message) {
  DCHECK(!browser_only_);
  auto message_ptr = blink::mojom::DevToolsMessage::New();
  message_ptr->data = mojo_base::BigBuffer(base::make_span(
      reinterpret_cast<const uint8_t*>(message.data()), message.length()));
  if (ShouldSendOnIO(method)) {
    if (io_session_ptr_)
      io_session_ptr_->DispatchProtocolCommand(call_id, method,
                                               std::move(message_ptr));
  } else {
    if (session_ptr_)
      session_ptr_->DispatchProtocolCommand(call_id, method,
                                            std::move(message_ptr));
  }
}

void DevToolsSession::SuspendSendingMessagesToAgent() {
  DCHECK(!browser_only_);
  suspended_sending_messages_to_agent_ = true;
}

void DevToolsSession::ResumeSendingMessagesToAgent() {
  DCHECK(!browser_only_);
  suspended_sending_messages_to_agent_ = false;
  for (const SuspendedMessage& message : suspended_messages_) {
    DispatchProtocolMessageToAgent(message.call_id, message.method,
                                   message.message);
    waiting_for_response_messages_[message.call_id] = {message.method,
                                                       message.message};
  }
  suspended_messages_.clear();
}

// The following methods handle responses or notifications coming from
// the browser to the client.
static void SendProtocolResponseOrNotification(
    DevToolsAgentHostClient* client,
    DevToolsAgentHostImpl* agent_host,
    std::unique_ptr<protocol::Serializable> message) {
  std::string cbor = message->serialize(/*binary=*/true);
  DCHECK(IsCBORMessage(SpanFrom(cbor)));
  if (client->UsesBinaryProtocol()) {
    client->DispatchProtocolMessage(agent_host, cbor);
    return;
  }
  std::string json;
  IPEStatus status = ConvertCBORToJSON(SpanFrom(cbor), &json);
  LOG_IF(ERROR, !status.ok()) << status.ToASCIIString();
  client->DispatchProtocolMessage(agent_host, json);
}

void DevToolsSession::sendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  SendProtocolResponseOrNotification(client_, agent_host_, std::move(message));
  // |this| may be deleted at this point.
}

void DevToolsSession::sendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  SendProtocolResponseOrNotification(client_, agent_host_, std::move(message));
  // |this| may be deleted at this point.
}

void DevToolsSession::flushProtocolNotifications() {
}

// The following methods handle responses or notifications coming from
// the renderer (blink) to the client.
static void DispatchProtocolResponseOrNotification(
    DevToolsAgentHostClient* client,
    DevToolsAgentHostImpl* agent_host,
    blink::mojom::DevToolsMessagePtr message) {
  client->DispatchProtocolMessage(
      agent_host,
      std::string(reinterpret_cast<const char*>(message->data.data()),
                  message->data.size()));
}

void DevToolsSession::DispatchProtocolResponse(
    blink::mojom::DevToolsMessagePtr message,
    int call_id,
    blink::mojom::DevToolsSessionStatePtr updates) {
  ApplySessionStateUpdates(std::move(updates));
  waiting_for_response_messages_.erase(call_id);
  DispatchProtocolResponseOrNotification(client_, agent_host_,
                                         std::move(message));
  // |this| may be deleted at this point.
}

void DevToolsSession::DispatchProtocolNotification(
    blink::mojom::DevToolsMessagePtr message,
    blink::mojom::DevToolsSessionStatePtr updates) {
  ApplySessionStateUpdates(std::move(updates));
  DispatchProtocolResponseOrNotification(client_, agent_host_,
                                         std::move(message));
  // |this| may be deleted at this point.
}

void DevToolsSession::DispatchOnClientHost(const std::string& message) {
  // |message| either comes from a web socket, in which case it's JSON.
  // Or it comes from another devtools_session, in which case it may be CBOR
  // already. We auto-detect and convert to what the client wants as needed.
  inspector_protocol_encoding::span<uint8_t> bytes = SpanFrom(message);
  bool is_cbor_message = IsCBORMessage(bytes);
  if (client_->UsesBinaryProtocol() == is_cbor_message) {
    client_->DispatchProtocolMessage(agent_host_, message);
    return;
  }
  std::string converted;
  IPEStatus status = client_->UsesBinaryProtocol()
                         ? ConvertJSONToCBOR(bytes, &converted)
                         : ConvertCBORToJSON(bytes, &converted);
  LOG_IF(ERROR, !status.ok()) << status.ToASCIIString();
  client_->DispatchProtocolMessage(agent_host_, converted);
  // |this| may be deleted at this point.
}

void DevToolsSession::ConnectionClosed() {
  DevToolsAgentHostClient* client = client_;
  DevToolsAgentHostImpl* agent_host = agent_host_;
  agent_host->DetachInternal(this);
  // |this| is delete here, do not use any fields below.
  client->AgentHostClosed(agent_host);
}

void DevToolsSession::ApplySessionStateUpdates(
    blink::mojom::DevToolsSessionStatePtr updates) {
  if (!updates)
    return;
  if (!session_state_cookie_)
    session_state_cookie_ = blink::mojom::DevToolsSessionState::New();
  for (auto& entry : updates->entries) {
    if (entry.second.has_value())
      session_state_cookie_->entries[entry.first] = std::move(*entry.second);
    else
      session_state_cookie_->entries.erase(entry.first);
  }
}

DevToolsSession* DevToolsSession::AttachChildSession(
    const std::string& session_id,
    DevToolsAgentHostImpl* agent_host,
    DevToolsAgentHostClient* client) {
  DCHECK(!agent_host->SessionByClient(client));
  DCHECK(!root_session_);
  auto session = std::make_unique<DevToolsSession>(client);
  session->root_session_ = this;
  DevToolsSession* session_ptr = session.get();
  // If attach did not succeed, |session| is already destroyed.
  if (!agent_host->AttachInternal(std::move(session)))
    return nullptr;
  child_sessions_[session_id] = session_ptr;
  return session_ptr;
}

void DevToolsSession::DetachChildSession(const std::string& session_id) {
  child_sessions_.erase(session_id);
}

void DevToolsSession::SendMessageFromChildSession(const std::string& session_id,
                                                  const std::string& message) {
  if (child_sessions_.find(session_id) == child_sessions_.end())
    return;
  DCHECK(IsCBORMessage(SpanFrom(message)));
  std::string patched(message);
  IPEStatus status = AppendString8EntryToCBORMap(
      SpanFrom(kSessionId), SpanFrom(session_id), &patched);
  LOG_IF(ERROR, !status.ok()) << status.ToASCIIString();
  if (!status.ok())
    return;
  if (client_->UsesBinaryProtocol()) {
    client_->DispatchProtocolMessage(agent_host_, patched);
    return;
  }
  std::string json;
  status = ConvertCBORToJSON(SpanFrom(patched), &json);
  LOG_IF(ERROR, !status.ok()) << status.ToASCIIString();
  client_->DispatchProtocolMessage(agent_host_, json);
  // |this| may be deleted at this point.
}

}  // namespace content
