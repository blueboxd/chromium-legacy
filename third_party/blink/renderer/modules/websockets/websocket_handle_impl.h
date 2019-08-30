/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_WEBSOCKET_HANDLE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_WEBSOCKET_HANDLE_IMPL_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/websocket.mojom-blink.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom-blink.h"
#include "third_party/blink/renderer/modules/websockets/websocket_handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace base {
class Location;
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {

class WebSocketHandleImpl
    : public WebSocketHandle,
      public network::mojom::blink::WebSocketHandshakeClient,
      public network::mojom::blink::WebSocketClient {
 public:
  explicit WebSocketHandleImpl(scoped_refptr<base::SingleThreadTaskRunner>);
  ~WebSocketHandleImpl() override;

  void Connect(mojo::Remote<mojom::blink::WebSocketConnector>,
               const KURL&,
               const Vector<String>& protocols,
               const KURL& site_for_cookies,
               const String& user_agent_override,
               WebSocketChannelImpl*) override;
  void Send(bool fin, MessageType, const char* data, wtf_size_t) override;
  void StartReceiving() override;
  void ConsumePendingDataFrames() override;
  void Close(uint16_t code, const String& reason) override;

 private:
  struct DataFrame final {
    DataFrame(bool fin,
              network::mojom::blink::WebSocketMessageType type,
              uint32_t data_length)
        : fin(fin), type(type), data_length(data_length) {}

    bool fin;
    network::mojom::blink::WebSocketMessageType type;
    uint32_t data_length;
  };

  void Disconnect();
  void OnConnectionError(const base::Location& set_from,
                         uint32_t custom_reason,
                         const std::string& description);

  // network::mojom::blink::WebSocketHandshakeClient methods:
  void OnOpeningHandshakeStarted(
      network::mojom::blink::WebSocketHandshakeRequestPtr) override;
  void OnResponseReceived(
      network::mojom::blink::WebSocketHandshakeResponsePtr) override;
  void OnConnectionEstablished(
      mojo::PendingRemote<network::mojom::blink::WebSocket> websocket,
      mojo::PendingReceiver<network::mojom::blink::WebSocketClient>
          client_receiver,
      const String& selected_protocol,
      const String& extensions,
      mojo::ScopedDataPipeConsumerHandle readable) override;

  // network::mojom::blink::WebSocketClient methods:
  void OnDataFrame(bool fin,
                   network::mojom::blink::WebSocketMessageType,
                   uint64_t data_length) override;
  void AddSendFlowControlQuota(int64_t quota) override;
  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const String& reason) override;
  void OnClosingHandshake() override;

  // Datapipe functions to receive.
  void OnReadable(MojoResult result, const mojo::HandleSignalsState& state);
  // Returns false if |this| is deleted.
  bool ConsumeDataFrame(bool fin,
                        network::mojom::blink::WebSocketMessageType type,
                        const void* data,
                        size_t data_size);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  WeakPersistent<WebSocketChannelImpl> channel_;

  mojo::Remote<network::mojom::blink::WebSocket> websocket_;
  mojo::Receiver<network::mojom::blink::WebSocketHandshakeClient>
      handshake_client_receiver_{this};
  mojo::Receiver<network::mojom::blink::WebSocketClient> client_receiver_{this};

  // Datapipe fields to receive.
  mojo::ScopedDataPipeConsumerHandle readable_;
  mojo::SimpleWatcher readable_watcher_;
  WTF::Deque<DataFrame> pending_data_frames_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_WEBSOCKET_HANDLE_IMPL_H_
