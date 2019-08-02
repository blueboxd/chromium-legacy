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

#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/websocket.mojom-blink.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom-blink.h"
#include "third_party/blink/renderer/modules/websockets/websocket_handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace base {
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

  void Connect(mojom::blink::WebSocketConnectorPtr,
               const KURL&,
               const Vector<String>& protocols,
               const KURL& site_for_cookies,
               const String& user_agent_override,
               WebSocketChannelImpl*) override;
  void Send(bool fin, MessageType, const char* data, wtf_size_t) override;
  void AddReceiveFlowControlQuota(int64_t quota) override;
  void Close(uint16_t code, const String& reason) override;

 private:
  void Disconnect();
  void OnConnectionError(uint32_t custom_reason,
                         const std::string& description);

  // network::mojom::blink::WebSocketHandshakeClient methods:
  void OnOpeningHandshakeStarted(
      network::mojom::blink::WebSocketHandshakeRequestPtr) override;
  void OnResponseReceived(
      network::mojom::blink::WebSocketHandshakeResponsePtr) override;
  void OnConnectionEstablished(network::mojom::blink::WebSocketPtr websocket,
                               const String& selected_protocol,
                               const String& extensions,
                               uint64_t receive_quota_threshold) override;
  // network::mojom::blink::WebSocketClient methods:
  void OnDataFrame(bool fin,
                   network::mojom::blink::WebSocketMessageType,
                   base::span<const uint8_t> data) override;
  void AddSendFlowControlQuota(int64_t quota) override;
  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const String& reason) override;
  void OnClosingHandshake() override;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  WeakPersistent<WebSocketChannelImpl> channel_;

  network::mojom::blink::WebSocketPtr websocket_;
  network::mojom::blink::WebSocketClientRequest pending_client_receiver_;
  mojo::Binding<network::mojom::blink::WebSocketHandshakeClient>
      handshake_client_binding_;
  mojo::Binding<network::mojom::blink::WebSocketClient> client_binding_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_WEBSOCKET_HANDLE_IMPL_H_
