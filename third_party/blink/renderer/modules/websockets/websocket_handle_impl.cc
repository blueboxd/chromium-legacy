// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/websocket_handle_impl.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"
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
      client_binding_(this),
      readable_watcher_(FROM_HERE,
                        mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                        task_runner_) {
  NETWORK_DVLOG(1) << this << " created";
}

WebSocketHandleImpl::~WebSocketHandleImpl() {
  NETWORK_DVLOG(1) << this << " deleted";

  if (websocket_)
    websocket_->StartClosingHandshake(kAbnormalShutdownOpCode, g_empty_string);
}

void WebSocketHandleImpl::Connect(
    mojo::Remote<mojom::blink::WebSocketConnector> connector,
    const KURL& url,
    const Vector<String>& protocols,
    const KURL& site_for_cookies,
    const String& user_agent_override,
    WebSocketChannelImpl* channel) {
  NETWORK_DVLOG(1) << this << " connect(" << url.GetString() << ")";

  DCHECK(!channel_);
  DCHECK(channel);
  channel_ = channel;

  connector->Connect(
      url, protocols, site_for_cookies, user_agent_override,
      handshake_client_receiver_.BindNewPipeAndPassRemote(task_runner_));
  handshake_client_receiver_.set_disconnect_with_reason_handler(
      WTF::Bind(&WebSocketHandleImpl::OnConnectionError, WTF::Unretained(this),
                FROM_HERE));
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

void WebSocketHandleImpl::StartReceiving() {
  websocket_->StartReceiving();
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

void WebSocketHandleImpl::OnConnectionError(const base::Location& set_from,
                                            uint32_t custom_reason,
                                            const std::string& description) {
  NETWORK_DVLOG(1) << " OnConnectionError("
                   << " reason: " << custom_reason
                   << ", description:" << description
                   << "), set_from:" << set_from.ToString();
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
    mojo::PendingReceiver<network::mojom::blink::WebSocketClient>
        client_receiver,
    const String& protocol,
    const String& extensions,
    mojo::ScopedDataPipeConsumerHandle readable) {
  NETWORK_DVLOG(1) << this << " OnConnectionEstablished(" << protocol << ", "
                   << extensions << ")";

  if (!channel_)
    return;

  // From now on, we will detect mojo errors via |client_binding_|.
  handshake_client_receiver_.reset();
  client_binding_.Bind(std::move(client_receiver), task_runner_);
  client_binding_.set_connection_error_with_reason_handler(
      WTF::Bind(&WebSocketHandleImpl::OnConnectionError, WTF::Unretained(this),
                FROM_HERE));

  DCHECK(!websocket_);
  websocket_ = std::move(websocket);
  readable_ = std::move(readable);
  const MojoResult mojo_result = readable_watcher_.Watch(
      readable_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      WTF::BindRepeating(&WebSocketHandleImpl::OnReadable,
                         WTF::Unretained(this)));
  DCHECK_EQ(mojo_result, MOJO_RESULT_OK);
  channel_->DidConnect(this, protocol, extensions);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnReadable(MojoResult result,
                                     const mojo::HandleSignalsState& state) {
  NETWORK_DVLOG(2) << this << " OnReadble mojo_result=" << result;
  if (result != MOJO_RESULT_OK) {
    if (channel_) {
      channel_->DidFail(this, "Unknown reason");
    } else {
      Disconnect();
    }
    return;
  }
  ConsumePendingDataFrames();
}

void WebSocketHandleImpl::OnDataFrame(
    bool fin,
    network::mojom::blink::WebSocketMessageType type,
    uint64_t data_length) {
  NETWORK_DVLOG(1) << this << " OnDataFrame(" << fin << ", " << type << ", "
                   << "(data_length = " << data_length << "))";
  pending_data_frames_.push_back(
      DataFrame(fin, type, static_cast<uint32_t>(data_length)));
  ConsumePendingDataFrames();
}

void WebSocketHandleImpl::ConsumePendingDataFrames() {
  DCHECK(channel_);
  if (channel_->HasBackPressureToReceiveData())
    return;
  while (!pending_data_frames_.empty() &&
         !channel_->HasBackPressureToReceiveData()) {
    DataFrame& data_frame = pending_data_frames_.front();
    NETWORK_DVLOG(2) << " ConsumePendingDataFrame frame=(" << data_frame.fin
                     << ", " << data_frame.type
                     << ", (data_length = " << data_frame.data_length << "))";
    if (data_frame.data_length == 0) {
      if (!ConsumeDataFrame(data_frame.fin, data_frame.type, nullptr, 0)) {
        // |this| is deleted.
        return;
      }
      pending_data_frames_.pop_front();
      continue;
    }

    const void* buffer;
    uint32_t readable_size;
    const MojoResult begin_result = readable_->BeginReadData(
        &buffer, &readable_size, MOJO_READ_DATA_FLAG_NONE);
    if (begin_result == MOJO_RESULT_SHOULD_WAIT) {
      readable_watcher_.ArmOrNotify();
      return;
    }
    if (begin_result == MOJO_RESULT_FAILED_PRECONDITION) {
      // |client_binding_| will catch the connection error.
      return;
    }
    DCHECK_EQ(begin_result, MOJO_RESULT_OK);

    if (readable_size >= data_frame.data_length) {
      if (!ConsumeDataFrame(data_frame.fin, data_frame.type, buffer,
                            data_frame.data_length)) {
        // |this| is deleted.
        return;
      }
      const MojoResult end_result =
          readable_->EndReadData(data_frame.data_length);
      DCHECK_EQ(end_result, MOJO_RESULT_OK);
      pending_data_frames_.pop_front();
      continue;
    }

    DCHECK_LT(readable_size, data_frame.data_length);
    if (!ConsumeDataFrame(false, data_frame.type, buffer, readable_size)) {
      // |this| is deleted.
      return;
    }
    const MojoResult end_result = readable_->EndReadData(readable_size);
    DCHECK_EQ(end_result, MOJO_RESULT_OK);
    data_frame.type = network::mojom::blink::WebSocketMessageType::CONTINUATION;
    data_frame.data_length -= readable_size;
  }
}

bool WebSocketHandleImpl::ConsumeDataFrame(
    bool fin,
    network::mojom::blink::WebSocketMessageType type,
    const void* data,
    size_t data_size) {
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
  const char* data_to_pass = reinterpret_cast<const char*>(data);
  WebSocketChannelImpl* channel = channel_.Get();
  channel_->DidReceiveData(this, fin, type_to_pass, data_to_pass, data_size);
  // DidReceiveData can delete |this|.
  return channel->IsHandleAlive();
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
