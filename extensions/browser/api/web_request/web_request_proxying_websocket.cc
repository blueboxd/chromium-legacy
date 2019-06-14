// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_proxying_websocket.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_util.h"

namespace extensions {

WebRequestProxyingWebSocket::WebRequestProxyingWebSocket(
    int process_id,
    int render_frame_id,
    const url::Origin& origin,
    content::BrowserContext* browser_context,
    content::ResourceContext* resource_context,
    InfoMap* info_map,
    scoped_refptr<WebRequestAPI::RequestIDGenerator> request_id_generator,
    network::mojom::WebSocketPtr proxied_socket,
    network::mojom::WebSocketRequest proxied_request,
    network::mojom::AuthenticationHandlerRequest auth_request,
    network::mojom::TrustedHeaderClientRequest header_client_request,
    WebRequestAPI::ProxySet* proxies)
    : process_id_(process_id),
      render_frame_id_(render_frame_id),
      origin_(origin),
      browser_context_(browser_context),
      resource_context_(resource_context),
      info_map_(info_map),
      request_id_generator_(std::move(request_id_generator)),
      proxied_socket_(std::move(proxied_socket)),
      binding_as_websocket_(this),
      binding_as_client_(this),
      binding_as_auth_handler_(this),
      binding_as_header_client_(this),
      proxies_(proxies),
      weak_factory_(this) {
  binding_as_websocket_.Bind(std::move(proxied_request));
  binding_as_auth_handler_.Bind(std::move(auth_request));

  binding_as_websocket_.set_connection_error_handler(
      base::BindRepeating(&WebRequestProxyingWebSocket::OnError,
                          base::Unretained(this), net::ERR_FAILED));
  binding_as_auth_handler_.set_connection_error_handler(
      base::BindRepeating(&WebRequestProxyingWebSocket::OnError,
                          base::Unretained(this), net::ERR_FAILED));

  if (header_client_request)
    binding_as_header_client_.Bind(std::move(header_client_request));
}

WebRequestProxyingWebSocket::~WebRequestProxyingWebSocket() {
  // This is important to ensure that no outstanding blocking requests continue
  // to reference state owned by this object.
  if (info_) {
    ExtensionWebRequestEventRouter::GetInstance()->OnRequestWillBeDestroyed(
        browser_context_, &info_.value());
  }
  if (on_before_send_headers_callback_) {
    std::move(on_before_send_headers_callback_)
        .Run(net::ERR_ABORTED, base::nullopt);
  }
  if (on_headers_received_callback_) {
    std::move(on_headers_received_callback_)
        .Run(net::ERR_ABORTED, base::nullopt, GURL());
  }
}

void WebRequestProxyingWebSocket::AddChannelRequest(
    const GURL& url,
    const std::vector<std::string>& requested_protocols,
    const GURL& site_for_cookies,
    std::vector<network::mojom::HttpHeaderPtr> additional_headers,
    network::mojom::WebSocketHandshakeClientPtr handshake_client,
    network::mojom::WebSocketClientPtr client) {
  if (binding_as_client_.is_bound() || !handshake_client ||
      forwarding_handshake_client_ || !client || forwarding_client_) {
    // Illegal request.
    proxied_socket_ = nullptr;
    return;
  }

  request_.url = url;
  request_.site_for_cookies = site_for_cookies;
  request_.request_initiator = origin_;
  websocket_protocols_ = requested_protocols;
  uint64_t request_id = request_id_generator_->Generate();
  int routing_id = MSG_ROUTING_NONE;
  info_.emplace(
      WebRequestInfoInitParams(request_id, process_id_, render_frame_id_,
                               nullptr, routing_id, resource_context_, request_,
                               false /* is_download */, true /* is_async */));

  forwarding_handshake_client_ = std::move(handshake_client);
  forwarding_client_ = std::move(client);
  additional_headers_ = std::move(additional_headers);

  // If the header client will be used, we start the request immediately, and
  // OnBeforeSendHeaders and OnSendHeaders will be handled there. Otherwise,
  // send these events before the request starts.
  base::RepeatingCallback<void(int)> continuation;
  if (binding_as_header_client_) {
    continuation = base::BindRepeating(
        &WebRequestProxyingWebSocket::ContinueToStartRequest,
        weak_factory_.GetWeakPtr());
  } else {
    continuation = base::BindRepeating(
        &WebRequestProxyingWebSocket::OnBeforeRequestComplete,
        weak_factory_.GetWeakPtr());
  }

  // TODO(yhirano): Consider having throttling here (probably with aligned with
  // WebRequestProxyingURLLoaderFactory).
  bool should_collapse_initiator = false;
  int result = ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRequest(
      browser_context_, info_map_, &info_.value(), continuation, &redirect_url_,
      &should_collapse_initiator);

  // It doesn't make sense to collapse WebSocket requests since they won't be
  // associated with a DOM element.
  DCHECK(!should_collapse_initiator);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  if (result == net::ERR_IO_PENDING) {
    return;
  }

  DCHECK_EQ(net::OK, result);
  continuation.Run(net::OK);
}

void WebRequestProxyingWebSocket::SendFrame(
    bool fin,
    network::mojom::WebSocketMessageType type,
    const std::vector<uint8_t>& data) {
  proxied_socket_->SendFrame(fin, type, data);
}

void WebRequestProxyingWebSocket::AddReceiveFlowControlQuota(int64_t quota) {
  proxied_socket_->AddReceiveFlowControlQuota(quota);
}

void WebRequestProxyingWebSocket::StartClosingHandshake(
    uint16_t code,
    const std::string& reason) {
  proxied_socket_->StartClosingHandshake(code, reason);
}

void WebRequestProxyingWebSocket::OnStartOpeningHandshake(
    network::mojom::WebSocketHandshakeRequestPtr request) {
  DCHECK(forwarding_handshake_client_);
  forwarding_handshake_client_->OnStartOpeningHandshake(std::move(request));
}

void WebRequestProxyingWebSocket::OnFinishOpeningHandshake(
    network::mojom::WebSocketHandshakeResponsePtr response) {
  DCHECK(forwarding_handshake_client_);

  // response_.headers will be set in OnBeforeSendHeaders if
  // binding_as_header_client_ is set.
  if (!binding_as_header_client_) {
    response_.headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(base::StringPrintf(
            "HTTP/%d.%d %d %s", response->http_version.major_value(),
            response->http_version.minor_value(), response->status_code,
            response->status_text.c_str()));
    for (const auto& header : response->headers)
      response_.headers->AddHeader(header->name + ": " + header->value);
  }

  response_.remote_endpoint = response->remote_endpoint;

  // TODO(yhirano): OnFinishOpeningHandshake is called with the original
  // response headers. That means if OnHeadersReceived modified them the
  // renderer won't see that modification. This is the opposite of http(s)
  // requests.
  forwarding_handshake_client_->OnFinishOpeningHandshake(std::move(response));

  if (!binding_as_header_client_ || response_.headers) {
    ContinueToHeadersReceived();
  } else {
    waiting_for_header_client_headers_received_ = true;
  }
}

void WebRequestProxyingWebSocket::ContinueToHeadersReceived() {
  auto continuation = base::BindRepeating(
      &WebRequestProxyingWebSocket::OnHeadersReceivedComplete,
      weak_factory_.GetWeakPtr());
  int result = ExtensionWebRequestEventRouter::GetInstance()->OnHeadersReceived(
      browser_context_, info_map_, &info_.value(), continuation,
      response_.headers.get(), &override_headers_, &redirect_url_);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  PauseIncomingMethodCallProcessing();
  if (result == net::ERR_IO_PENDING)
    return;

  DCHECK_EQ(net::OK, result);
  OnHeadersReceivedComplete(net::OK);
}

void WebRequestProxyingWebSocket::OnAddChannelResponse(
    const std::string& selected_protocol,
    const std::string& extensions) {
  DCHECK(forwarding_handshake_client_);
  DCHECK(!is_done_);
  is_done_ = true;
  ExtensionWebRequestEventRouter::GetInstance()->OnCompleted(
      browser_context_, info_map_, &info_.value(), net::ERR_WS_UPGRADE);

  forwarding_handshake_client_->OnAddChannelResponse(selected_protocol,
                                                     extensions);
}

void WebRequestProxyingWebSocket::OnAuthRequired(
    const net::AuthChallengeInfo& auth_info,
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    const net::IPEndPoint& remote_endpoint,
    OnAuthRequiredCallback callback) {
  if (!callback) {
    OnError(net::ERR_FAILED);
    return;
  }

  response_.headers = headers;
  response_.remote_endpoint = remote_endpoint;
  auth_required_callback_ = std::move(callback);

  auto continuation = base::BindRepeating(
      &WebRequestProxyingWebSocket::OnHeadersReceivedCompleteForAuth,
      weak_factory_.GetWeakPtr(), auth_info);
  int result = ExtensionWebRequestEventRouter::GetInstance()->OnHeadersReceived(
      browser_context_, info_map_, &info_.value(), continuation,
      response_.headers.get(), &override_headers_, &redirect_url_);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  PauseIncomingMethodCallProcessing();
  if (result == net::ERR_IO_PENDING)
    return;

  DCHECK_EQ(net::OK, result);
  OnHeadersReceivedCompleteForAuth(auth_info, net::OK);
}

void WebRequestProxyingWebSocket::OnBeforeSendHeaders(
    const net::HttpRequestHeaders& headers,
    OnBeforeSendHeadersCallback callback) {
  DCHECK(binding_as_header_client_);

  request_.headers = headers;
  on_before_send_headers_callback_ = std::move(callback);
  OnBeforeRequestComplete(net::OK);
}

void WebRequestProxyingWebSocket::OnHeadersReceived(
    const std::string& headers,
    OnHeadersReceivedCallback callback) {
  DCHECK(binding_as_header_client_);

  // Note: since there are different pipes used for WebSocketClient and
  // TrustedHeaderClient, there are no guarantees whether this or
  // OnFinishOpeningHandshake are called first.
  on_headers_received_callback_ = std::move(callback);
  response_.headers = base::MakeRefCounted<net::HttpResponseHeaders>(headers);

  if (!waiting_for_header_client_headers_received_)
    return;

  waiting_for_header_client_headers_received_ = false;
  ContinueToHeadersReceived();
}

void WebRequestProxyingWebSocket::StartProxying(
    int process_id,
    int render_frame_id,
    scoped_refptr<WebRequestAPI::RequestIDGenerator> request_id_generator,
    const url::Origin& origin,
    content::BrowserContext* browser_context,
    content::ResourceContext* resource_context,
    InfoMap* info_map,
    network::mojom::WebSocketPtrInfo proxied_socket_ptr_info,
    network::mojom::WebSocketRequest proxied_request,
    network::mojom::AuthenticationHandlerRequest auth_request,
    network::mojom::TrustedHeaderClientRequest header_client_request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto* proxies =
      WebRequestAPI::ProxySet::GetFromResourceContext(resource_context);

  auto proxy = std::make_unique<WebRequestProxyingWebSocket>(
      process_id, render_frame_id, origin, browser_context, resource_context,
      info_map, std::move(request_id_generator),
      network::mojom::WebSocketPtr(std::move(proxied_socket_ptr_info)),
      std::move(proxied_request), std::move(auth_request),
      std::move(header_client_request), proxies);

  proxies->AddProxy(std::move(proxy));
}

void WebRequestProxyingWebSocket::OnBeforeRequestComplete(int error_code) {
  DCHECK(binding_as_header_client_ || !binding_as_client_.is_bound());
  DCHECK(request_.url.SchemeIsWSOrWSS());
  if (error_code != net::OK) {
    OnError(error_code);
    return;
  }

  auto continuation = base::BindRepeating(
      &WebRequestProxyingWebSocket::OnBeforeSendHeadersComplete,
      weak_factory_.GetWeakPtr());

  int result =
      ExtensionWebRequestEventRouter::GetInstance()->OnBeforeSendHeaders(
          browser_context_, info_map_, &info_.value(), continuation,
          &request_.headers);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  if (result == net::ERR_IO_PENDING)
    return;

  DCHECK_EQ(net::OK, result);
  OnBeforeSendHeadersComplete(std::set<std::string>(), std::set<std::string>(),
                              net::OK);
}

void WebRequestProxyingWebSocket::OnBeforeSendHeadersComplete(
    const std::set<std::string>& removed_headers,
    const std::set<std::string>& set_headers,
    int error_code) {
  DCHECK(binding_as_header_client_ || !binding_as_client_.is_bound());
  if (error_code != net::OK) {
    OnError(error_code);
    return;
  }

  if (binding_as_header_client_) {
    DCHECK(on_before_send_headers_callback_);
    std::move(on_before_send_headers_callback_)
        .Run(error_code, request_.headers);
  }

  ExtensionWebRequestEventRouter::GetInstance()->OnSendHeaders(
      browser_context_, info_map_, &info_.value(), request_.headers);

  if (!binding_as_header_client_)
    ContinueToStartRequest(net::OK);
}

void WebRequestProxyingWebSocket::ContinueToStartRequest(int error_code) {
  network::mojom::WebSocketHandshakeClientPtr proxy;

  base::flat_set<std::string> used_header_names;
  std::vector<network::mojom::HttpHeaderPtr> additional_headers;
  for (net::HttpRequestHeaders::Iterator it(request_.headers); it.GetNext();) {
    additional_headers.push_back(
        network::mojom::HttpHeader::New(it.name(), it.value()));
    used_header_names.insert(base::ToLowerASCII(it.name()));
  }
  for (const auto& header : additional_headers_) {
    if (!used_header_names.contains(base::ToLowerASCII(header->name))) {
      additional_headers.push_back(
          network::mojom::HttpHeader::New(header->name, header->value));
    }
  }

  binding_as_client_.Bind(mojo::MakeRequest(&proxy));
  binding_as_client_.set_connection_error_handler(
      base::BindOnce(&WebRequestProxyingWebSocket::OnError,
                     base::Unretained(this), net::ERR_FAILED));
  proxied_socket_->AddChannelRequest(
      request_.url, websocket_protocols_, request_.site_for_cookies,
      std::move(additional_headers), std::move(proxy),
      std::move(forwarding_client_));
}

void WebRequestProxyingWebSocket::OnHeadersReceivedComplete(int error_code) {
  if (error_code != net::OK) {
    OnError(error_code);
    return;
  }

  if (on_headers_received_callback_) {
    base::Optional<std::string> headers;
    if (override_headers_)
      headers = override_headers_->raw_headers();
    std::move(on_headers_received_callback_).Run(net::OK, headers, GURL());
  }

  if (override_headers_) {
    response_.headers = override_headers_;
    override_headers_ = nullptr;
  }

  ResumeIncomingMethodCallProcessing();
  info_->AddResponseInfoFromResourceResponse(response_);
  ExtensionWebRequestEventRouter::GetInstance()->OnResponseStarted(
      browser_context_, info_map_, &info_.value(), net::OK);
}

void WebRequestProxyingWebSocket::OnAuthRequiredComplete(
    net::NetworkDelegate::AuthRequiredResponse rv) {
  DCHECK(auth_required_callback_);
  ResumeIncomingMethodCallProcessing();
  switch (rv) {
    case net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION:
    case net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH:
      std::move(auth_required_callback_).Run(base::nullopt);
      break;

    case net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH:
      std::move(auth_required_callback_).Run(auth_credentials_);
      break;
    case net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING:
      NOTREACHED();
      break;
  }
}

void WebRequestProxyingWebSocket::OnHeadersReceivedCompleteForAuth(
    const net::AuthChallengeInfo& auth_info,
    int rv) {
  if (rv != net::OK) {
    OnError(rv);
    return;
  }
  ResumeIncomingMethodCallProcessing();
  info_->AddResponseInfoFromResourceResponse(response_);

  auto continuation =
      base::BindRepeating(&WebRequestProxyingWebSocket::OnAuthRequiredComplete,
                          weak_factory_.GetWeakPtr());
  auto auth_rv = ExtensionWebRequestEventRouter::GetInstance()->OnAuthRequired(
      browser_context_, info_map_, &info_.value(), auth_info,
      std::move(continuation), &auth_credentials_);
  PauseIncomingMethodCallProcessing();
  if (auth_rv == net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING)
    return;

  OnAuthRequiredComplete(auth_rv);
}

void WebRequestProxyingWebSocket::PauseIncomingMethodCallProcessing() {
  binding_as_client_.PauseIncomingMethodCallProcessing();
  binding_as_auth_handler_.PauseIncomingMethodCallProcessing();
  if (binding_as_header_client_)
    binding_as_header_client_.PauseIncomingMethodCallProcessing();
}

void WebRequestProxyingWebSocket::ResumeIncomingMethodCallProcessing() {
  binding_as_client_.ResumeIncomingMethodCallProcessing();
  binding_as_auth_handler_.ResumeIncomingMethodCallProcessing();
  if (binding_as_header_client_)
    binding_as_header_client_.ResumeIncomingMethodCallProcessing();
}

void WebRequestProxyingWebSocket::OnError(int error_code) {
  if (!is_done_ && info_.has_value()) {
    is_done_ = true;
    ExtensionWebRequestEventRouter::GetInstance()->OnErrorOccurred(
        browser_context_, info_map_, &info_.value(), true /* started */,
        error_code);
  }

  // Deletes |this|.
  proxies_->RemoveProxy(this);
}

}  // namespace extensions
