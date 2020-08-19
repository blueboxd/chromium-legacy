// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/websocket_adapter.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/fido_constants.h"

namespace device {
namespace cablev2 {

// kMaxIncomingMessageSize is the maximum number of bytes in a single message
// from a WebSocket. This is set to be far larger than any plausible CTAP2
// message and exists to prevent a run away server from using up all memory.
static constexpr size_t kMaxIncomingMessageSize = 1 << 20;

WebSocketAdapter::WebSocketAdapter(TunnelReadyCallback on_tunnel_ready,
                                   TunnelDataCallback on_tunnel_data)
    : on_tunnel_ready_(std::move(on_tunnel_ready)),
      on_tunnel_data_(std::move(on_tunnel_data)) {}

WebSocketAdapter::~WebSocketAdapter() = default;

mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
WebSocketAdapter::BindNewHandshakeClientPipe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto ret = handshake_receiver_.BindNewPipeAndPassRemote();
  handshake_receiver_.set_disconnect_handler(base::BindOnce(
      &WebSocketAdapter::OnMojoPipeDisconnect, base::Unretained(this)));
  return ret;
}

bool WebSocketAdapter::Write(base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (closed_ || data.size() > std::numeric_limits<uint32_t>::max()) {
    return false;
  }
  socket_remote_->SendMessage(network::mojom::WebSocketMessageType::BINARY,
                              data.size());
  uint32_t num_bytes = static_cast<uint32_t>(data.size());
  MojoResult result = write_pipe_->WriteData(data.data(), &num_bytes,
                                             MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  DCHECK(result != MOJO_RESULT_OK ||
         data.size() == static_cast<size_t>(num_bytes));
  return result == MOJO_RESULT_OK;
}

void WebSocketAdapter::OnOpeningHandshakeStarted(
    network::mojom::WebSocketHandshakeRequestPtr request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebSocketAdapter::OnConnectionEstablished(
    mojo::PendingRemote<network::mojom::WebSocket> socket,
    mojo::PendingReceiver<network::mojom::WebSocketClient> client_receiver,
    network::mojom::WebSocketHandshakeResponsePtr response,
    mojo::ScopedDataPipeConsumerHandle readable,
    mojo::ScopedDataPipeProducerHandle writable) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (response->selected_protocol != kCableWebSocketProtocol) {
    FIDO_LOG(ERROR) << "Tunnel server didn't select cable protocol";
    return;
  }

  base::Optional<uint8_t> shard_id;
  for (const auto& header : response->headers) {
    if (base::EqualsCaseInsensitiveASCII(header->name.c_str(),
                                         kCableShardIdHeader)) {
      unsigned u;
      if (!base::StringToUint(header->value, &u) || shard_id > 63) {
        FIDO_LOG(ERROR) << "Invalid shard ID from tunnel server";
        return;
      }
      shard_id = u;
      break;
    }
  }

  socket_remote_.Bind(std::move(socket));
  read_pipe_ = std::move(readable);
  write_pipe_ = std::move(writable);
  client_receiver_.Bind(std::move(client_receiver));
  socket_remote_->StartReceiving();

  std::move(on_tunnel_ready_).Run(true, shard_id);
}

void WebSocketAdapter::OnDataFrame(bool finish,
                                   network::mojom::WebSocketMessageType type,
                                   uint64_t data_len) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const size_t old_size = pending_message_.size();
  const size_t new_size = old_size + data_len;
  if (type != network::mojom::WebSocketMessageType::BINARY ||
      data_len > std::numeric_limits<uint32_t>::max() || new_size < old_size ||
      new_size > kMaxIncomingMessageSize) {
    FIDO_LOG(ERROR) << "invalid WebSocket frame";
    Close();
    return;
  }

  if (data_len > 0) {
    pending_message_.resize(new_size);
    uint32_t data_len_32 = data_len;
    if (read_pipe_->ReadData(&pending_message_.data()[old_size], &data_len_32,
                             MOJO_READ_DATA_FLAG_ALL_OR_NONE) !=
        MOJO_RESULT_OK) {
      FIDO_LOG(ERROR) << "reading WebSocket frame failed";
      Close();
      return;
    }
    DCHECK_EQ(static_cast<size_t>(data_len_32), data_len);
  }

  if (finish) {
    on_tunnel_data_.Run(pending_message_);
    pending_message_.resize(0);
  }
}

void WebSocketAdapter::OnDropChannel(bool was_clean,
                                     uint16_t code,
                                     const std::string& reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Close();
}

void WebSocketAdapter::OnClosingHandshake() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebSocketAdapter::OnMojoPipeDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If disconnection happens before |OnConnectionEstablished| then report a
  // failure to establish the tunnel.
  if (on_tunnel_ready_) {
    std::move(on_tunnel_ready_).Run(false, base::nullopt);
    return;
  }

  // Otherwise, act as if the TLS connection was closed.
  if (!closed_) {
    Close();
  }
}

void WebSocketAdapter::Close() {
  DCHECK(!closed_);
  closed_ = true;
  client_receiver_.reset();
  on_tunnel_data_.Run(base::nullopt);
}

}  // namespace cablev2
}  // namespace device
