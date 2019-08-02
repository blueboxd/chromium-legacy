// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/websocket_handle_impl.h"

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel_impl.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/network_log.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

const uint16_t kAbnormalShutdownOpCode = 1006;

}  // namespace

WebSocketHandleImpl::WebSocketHandleImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      channel_(nullptr),
      handshake_client_binding_(this),
      client_binding_(this) {
  NETWORK_DVLOG(1) << this << " created";
}

WebSocketHandleImpl::~WebSocketHandleImpl() {
  NETWORK_DVLOG(1) << this << " deleted";

  if (websocket_)
    websocket_->StartClosingHandshake(kAbnormalShutdownOpCode, g_empty_string);
}

void WebSocketHandleImpl::Connect(mojom::blink::WebSocketConnectorPtr connector,
                                  const KURL& url,
                                  const Vector<String>& protocols,
                                  const KURL& site_for_cookies,
                                  const String& user_agent_override,
                                  WebSocketChannelImpl* channel) {
  NETWORK_DVLOG(1) << this << " connect(" << url.GetString() << ")";

  DCHECK(!channel_);
  DCHECK(channel);
  channel_ = channel;

  Vector<network::mojom::blink::HttpHeaderPtr> additional_headers;
  network::mojom::blink::WebSocketHandshakeClientPtr handshake_client_proxy;
  handshake_client_binding_.Bind(
      mojo::MakeRequest(&handshake_client_proxy, task_runner_), task_runner_);
  handshake_client_binding_.set_connection_error_with_reason_handler(WTF::Bind(
      &WebSocketHandleImpl::OnConnectionError, WTF::Unretained(this)));
  network::mojom::blink::WebSocketClientPtr client_proxy;
  pending_client_receiver_ = mojo::MakeRequest(&client_proxy);

  connector->Connect(url, protocols, site_for_cookies, user_agent_override,
                     std::move(handshake_client_proxy),
                     std::move(client_proxy));
}

void WebSocketHandleImpl::Send(bool fin,
                               WebSocketHandle::MessageType type,
                               const char* data,
                               wtf_size_t size) {
  DCHECK(websocket_);

  network::mojom::blink::WebSocketMessageType type_to_pass;
  switch (type) {
    case WebSocketHandle::kMessageTypeContinuation:
      type_to_pass = network::mojom::blink::WebSocketMessageType::CONTINUATION;
      break;
    case WebSocketHandle::kMessageTypeText:
      type_to_pass = network::mojom::blink::WebSocketMessageType::TEXT;
      break;
    case WebSocketHandle::kMessageTypeBinary:
      type_to_pass = network::mojom::blink::WebSocketMessageType::BINARY;
      break;
    default:
      NOTREACHED();
      return;
  }

  NETWORK_DVLOG(1) << this << " send(" << fin << ", " << type_to_pass << ", "
                   << "(data size = " << size << "))";

  // TODO(darin): Avoid this copy.
  Vector<uint8_t> data_to_pass(size);
  std::copy(data, data + size, data_to_pass.begin());

  websocket_->SendFrame(fin, type_to_pass, data_to_pass);
}

void WebSocketHandleImpl::AddReceiveFlowControlQuota(int64_t quota) {
  DCHECK(websocket_);

  NETWORK_DVLOG(1) << this << " flowControl(" << quota << ")";

  websocket_->AddReceiveFlowControlQuota(quota);
}

void WebSocketHandleImpl::Close(uint16_t code, const String& reason) {
  DCHECK(websocket_);

  NETWORK_DVLOG(1) << this << " close(" << code << ", " << reason << ")";

  websocket_->StartClosingHandshake(code,
                                    reason.IsNull() ? g_empty_string : reason);
}

void WebSocketHandleImpl::Disconnect() {
  websocket_.reset();
  channel_ = nullptr;
}

void WebSocketHandleImpl::OnConnectionError(uint32_t custom_reason,
                                            const std::string& description) {
  NETWORK_DVLOG(1) << " OnConnectionError( reason: " << custom_reason
                   << ", description:" << description;
  String message = "Unknown reason";
  if (custom_reason == network::mojom::blink::WebSocket::kInternalFailure) {
    message = String::FromUTF8(description.c_str(), description.size());
  }

  WebSocketChannelImpl* channel = channel_;
  Disconnect();
  if (!channel)
    return;

  channel->DidFail(this, message);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnOpeningHandshakeStarted(
    network::mojom::blink::WebSocketHandshakeRequestPtr request) {
  NETWORK_DVLOG(1) << this << " OnOpeningHandshakeStarted("
                   << request->url.GetString() << ")";
  channel_->DidStartOpeningHandshake(this, std::move(request));
}

void WebSocketHandleImpl::OnResponseReceived(
    network::mojom::blink::WebSocketHandshakeResponsePtr response) {
  NETWORK_DVLOG(1) << this << " OnResponseReceived("
                   << response->url.GetString() << ")";
  channel_->DidFinishOpeningHandshake(this, std::move(response));
}

void WebSocketHandleImpl::OnConnectionEstablished(
    network::mojom::blink::WebSocketPtr websocket,
    const String& protocol,
    const String& extensions,
    uint64_t receive_quota_threshold) {
  NETWORK_DVLOG(1) << this << " OnConnectionEstablished(" << protocol << ", "
                   << extensions << ", " << receive_quota_threshold << ")";

  if (!channel_)
    return;

  // From now on, we will detect mojo errors via |client_binding_|.
  handshake_client_binding_.Close();
  client_binding_.Bind(std::move(pending_client_receiver_), task_runner_);
  client_binding_.set_connection_error_with_reason_handler(WTF::Bind(
      &WebSocketHandleImpl::OnConnectionError, WTF::Unretained(this)));

  DCHECK(!websocket_);
  websocket_ = std::move(websocket);
  channel_->DidConnect(this, protocol, extensions, receive_quota_threshold);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnDataFrame(
    bool fin,
    network::mojom::blink::WebSocketMessageType type,
    base::span<const uint8_t> data) {
  NETWORK_DVLOG(1) << this << " OnDataFrame(" << fin << ", " << type << ", "
                   << "(data size = " << data.size() << "))";
  if (!channel_)
    return;

  WebSocketHandle::MessageType type_to_pass =
      WebSocketHandle::kMessageTypeContinuation;
  switch (type) {
    case network::mojom::blink::WebSocketMessageType::CONTINUATION:
      type_to_pass = WebSocketHandle::kMessageTypeContinuation;
      break;
    case network::mojom::blink::WebSocketMessageType::TEXT:
      type_to_pass = WebSocketHandle::kMessageTypeText;
      break;
    case network::mojom::blink::WebSocketMessageType::BINARY:
      type_to_pass = WebSocketHandle::kMessageTypeBinary;
      break;
  }
  const char* data_to_pass =
      reinterpret_cast<const char*>(data.empty() ? nullptr : data.data());
  channel_->DidReceiveData(this, fin, type_to_pass, data_to_pass, data.size());
  // |this| can be deleted here.
}

void WebSocketHandleImpl::AddSendFlowControlQuota(int64_t quota) {
  NETWORK_DVLOG(1) << this << " AddSendFlowControlQuota(" << quota << ")";
  if (!channel_)
    return;

  channel_->AddSendFlowControlQuota(this, quota);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnDropChannel(bool was_clean,
                                        uint16_t code,
                                        const String& reason) {
  NETWORK_DVLOG(1) << this << " OnDropChannel(" << was_clean << ", " << code
                   << ", " << reason << ")";

  WebSocketChannelImpl* channel = channel_;
  Disconnect();
  if (!channel)
    return;

  channel->DidClose(this, was_clean, code, reason);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnClosingHandshake() {
  NETWORK_DVLOG(1) << this << " OnClosingHandshake()";
  if (!channel_)
    return;

  channel_->DidStartClosingHandshake(this);
  // |this| can be deleted here.
}

}  // namespace blink
