// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEBSOCKET_H_
#define SERVICES_NETWORK_WEBSOCKET_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/websockets/websocket_event_interface.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "services/network/websocket_throttler.h"
#include "url/origin.h"

class GURL;

namespace net {
class URLRequestContext;
class WebSocketChannel;
class SSLInfo;
}  // namespace net

namespace network {

// Host of net::WebSocketChannel.
class COMPONENT_EXPORT(NETWORK_SERVICE) WebSocket : public mojom::WebSocket {
 public:
  class Delegate {
   public:
    enum class BadMessageReason {
      kUnexpectedAddChannelRequest,
      kUnexpectedSendFrame,
    };
    virtual ~Delegate() {}

    virtual net::URLRequestContext* GetURLRequestContext() = 0;
    // This function may delete |impl|.
    virtual void OnLostConnectionToClient(WebSocket* impl) = 0;
    virtual void OnSSLCertificateError(
        std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks>
            callbacks,
        const GURL& url,
        int child_id,
        int frame_id,
        int net_error,
        const net::SSLInfo& ssl_info,
        bool fatal) = 0;
    // This function may delete |impl|.
    virtual void ReportBadMessage(BadMessageReason reason, WebSocket* impl) = 0;
    virtual bool CanReadRawCookies(const GURL& url) = 0;
    virtual void OnCreateURLRequest(int child_id,
                                    int frame_id,
                                    net::URLRequest* request) = 0;
  };

  WebSocket(std::unique_ptr<Delegate> delegate,
            const GURL& url,
            const std::vector<std::string>& requested_protocols,
            const GURL& site_for_cookies,
            std::vector<mojom::HttpHeaderPtr> additional_headers,
            int32_t process_id,
            int32_t render_frame_id,
            const url::Origin& origin,
            uint32_t options,
            mojom::WebSocketHandshakeClientPtr handshake_client,
            mojom::WebSocketClientPtr client,
            mojom::AuthenticationHandlerPtr auth_handler,
            mojom::TrustedHeaderClientPtr header_client,
            WebSocketThrottler::PendingConnection pending_connection_tracker,
            base::TimeDelta delay);
  ~WebSocket() override;

  // The renderer process is going away.
  // This function is virtual for testing.
  virtual void GoAway();

  // mojom::WebSocket methods:
  void SendFrame(bool fin,
                 mojom::WebSocketMessageType type,
                 const std::vector<uint8_t>& data) override;
  void AddReceiveFlowControlQuota(int64_t quota) override;
  void StartClosingHandshake(uint16_t code, const std::string& reason) override;

  bool handshake_succeeded() const { return handshake_succeeded_; }

  // Whether to allow sending/setting cookies during WebSocket handshakes for
  // |url|. This decision is based on the |options_| and |origin_| this
  // WebSocket was created with.
  bool AllowCookies(const GURL& url) const;

  // These methods are called by the network delegate to forward these events to
  // the |header_client_|.
  int OnBeforeStartTransaction(net::CompletionOnceCallback callback,
                               net::HttpRequestHeaders* headers);
  int OnHeadersReceived(
      net::CompletionOnceCallback callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url);

  // Gets the WebSocket associated with this request.
  static WebSocket* ForRequest(const net::URLRequest& request);

  static const void* const kUserDataKey;

 protected:
  class WebSocketEventHandler;

  // This class is used to set the WebSocket as user data on a URLRequest. This
  // is used instead of WebSocket directly because SetUserData requires a
  // std::unique_ptr. This is safe because WebSocket owns the URLRequest, so is
  // guaranteed to outlive it.
  class UnownedPointer : public base::SupportsUserData::Data {
   public:
    explicit UnownedPointer(WebSocket* pointer) : pointer_(pointer) {}

    WebSocket* get() const { return pointer_; }

   private:
    WebSocket* const pointer_;

    DISALLOW_COPY_AND_ASSIGN(UnownedPointer);
  };

  void OnConnectionError();
  void AddChannel(const GURL& socket_url,
                  const std::vector<std::string>& requested_protocols,
                  const GURL& site_for_cookies,
                  std::vector<mojom::HttpHeaderPtr> additional_headers);
  void OnAuthRequiredComplete(
      base::OnceCallback<void(const net::AuthCredentials*)> callback,
      const base::Optional<net::AuthCredentials>& credential);
  void OnBeforeSendHeadersComplete(
      net::CompletionOnceCallback callback,
      net::HttpRequestHeaders* out_headers,
      int result,
      const base::Optional<net::HttpRequestHeaders>& headers);
  void OnHeadersReceivedComplete(
      net::CompletionOnceCallback callback,
      scoped_refptr<net::HttpResponseHeaders>* out_headers,
      GURL* out_allowed_unsafe_redirect_url,
      int result,
      const base::Optional<std::string>& headers,
      const GURL& allowed_unsafe_redirect_url);

  std::unique_ptr<Delegate> delegate_;
  mojo::Binding<mojom::WebSocket> binding_;

  mojom::WebSocketHandshakeClientPtr handshake_client_;
  mojom::WebSocketClientPtr client_;
  mojom::AuthenticationHandlerPtr auth_handler_;
  mojom::TrustedHeaderClientPtr header_client_;

  WebSocketThrottler::PendingConnection pending_connection_tracker_;

  // The channel we use to send events to the network.
  std::unique_ptr<net::WebSocketChannel> channel_;

  // Delay used for per-renderer WebSocket throttling.
  base::TimeDelta delay_;

  // AddReceiveFlowControlQuota() is delayed when OnFlowControl() is called
  // before AddChannel() is called.
  // Zero indicates there is no pending AddReceiveFlowControlQuota().
  int64_t pending_flow_control_quota_;

  uint32_t options_;

  int32_t child_id_;
  int32_t frame_id_;

  // The web origin to use for the WebSocket.
  const url::Origin origin_;

  // handshake_succeeded_ is used by WebSocketManager to manage counters for
  // per-renderer WebSocket throttling.
  bool handshake_succeeded_;

  base::WeakPtrFactory<WebSocket> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebSocket);
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEBSOCKET_H_
