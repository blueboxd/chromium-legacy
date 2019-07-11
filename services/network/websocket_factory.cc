// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/websocket_factory.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/url_util.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/websocket.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace network {

class WebSocketFactory::Delegate final : public WebSocket::Delegate {
 public:
  Delegate(WebSocketFactory* factory, int32_t process_id)
      : factory_(factory), process_id_(process_id) {}
  ~Delegate() override {}

  net::URLRequestContext* GetURLRequestContext() override {
    return factory_->context_->url_request_context();
  }

  void OnLostConnectionToClient(WebSocket* impl) override {
    factory_->OnLostConnectionToClient(impl);
  }

  void OnSSLCertificateError(
      std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks>
          callbacks,
      const GURL& url,
      int process_id,
      int render_frame_id,
      int net_error,
      const net::SSLInfo& ssl_info,
      bool fatal) override {
    DCHECK(!callbacks_);
    callbacks_ = std::move(callbacks);

    NetworkService* network_service = factory_->context_->network_service();
    network_service->client()->OnSSLCertificateError(
        process_id, render_frame_id, url, net_error, ssl_info, fatal,
        base::BindRepeating(&Delegate::OnSSLCertificateErrorResponse,
                            weak_factory_.GetWeakPtr(), ssl_info));
  }

  void ReportBadMessage(BadMessageReason reason, WebSocket* impl) override {
    OnLostConnectionToClient(impl);
  }

  bool CanReadRawCookies(const GURL& url) override {
    DCHECK(url.SchemeIsWSOrWSS());
    GURL url_to_check = net::ChangeWebSocketSchemeToHttpScheme(url);
    return factory_->context_->network_service()->HasRawHeadersAccess(
        process_id_, url_to_check);
  }

  void OnCreateURLRequest(int child_id,
                          int frame_id,
                          net::URLRequest* request) override {}

 private:
  void OnSSLCertificateErrorResponse(const net::SSLInfo& ssl_info,
                                     int net_error) {
    if (net_error == net::OK) {
      callbacks_->ContinueSSLRequest();
      return;
    }

    callbacks_->CancelSSLRequest(net_error, &ssl_info);
  }

  // |factory_| outlives this object.
  WebSocketFactory* const factory_;
  const int process_id_;
  std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks> callbacks_;

  base::WeakPtrFactory<Delegate> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

WebSocketFactory::WebSocketFactory(NetworkContext* context)
    : context_(context) {}

WebSocketFactory::~WebSocketFactory() {}

void WebSocketFactory::CreateWebSocket(
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
    mojom::TrustedHeaderClientPtr header_client) {
  if (throttler_.HasTooManyPendingConnections(process_id)) {
    // Too many websockets!
    client.ResetWithReason(
        mojom::WebSocket::kInsufficientResources,
        "Error in connection establishment: net::ERR_INSUFFICIENT_RESOURCES");
    return;
  }
  connections_.insert(std::make_unique<WebSocket>(
      std::make_unique<Delegate>(this, process_id), url, requested_protocols,
      site_for_cookies, std::move(additional_headers), process_id,
      render_frame_id, origin, options, std::move(handshake_client),
      std::move(client), std::move(auth_handler), std::move(header_client),
      throttler_.IssuePendingConnectionTracker(process_id),
      throttler_.CalculateDelay(process_id)));
}

void WebSocketFactory::OnLostConnectionToClient(WebSocket* impl) {
  auto it = connections_.find(impl);
  DCHECK(it != connections_.end());
  impl->GoAway();
  connections_.erase(it);
}

}  // namespace network
