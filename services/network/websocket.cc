// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/websocket.h"

#include <inttypes.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/auth.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_info.h"
#include "net/websockets/websocket_basic_stream.h"
#include "net/websockets/websocket_channel.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_frame.h"  // for WebSocketFrameHeader::OpCode
#include "net/websockets/websocket_handshake_request_info.h"
#include "net/websockets/websocket_handshake_response_info.h"

namespace network {
namespace {

// Convert a mojom::WebSocketMessageType to a
// net::WebSocketFrameHeader::OpCode
net::WebSocketFrameHeader::OpCode MessageTypeToOpCode(
    mojom::WebSocketMessageType type) {
  DCHECK(type == mojom::WebSocketMessageType::CONTINUATION ||
         type == mojom::WebSocketMessageType::TEXT ||
         type == mojom::WebSocketMessageType::BINARY);
  typedef net::WebSocketFrameHeader::OpCode OpCode;
  // These compile asserts verify that the same underlying values are used for
  // both types, so we can simply cast between them.
  static_assert(
      static_cast<OpCode>(mojom::WebSocketMessageType::CONTINUATION) ==
          net::WebSocketFrameHeader::kOpCodeContinuation,
      "enum values must match for opcode continuation");
  static_assert(static_cast<OpCode>(mojom::WebSocketMessageType::TEXT) ==
                    net::WebSocketFrameHeader::kOpCodeText,
                "enum values must match for opcode text");
  static_assert(static_cast<OpCode>(mojom::WebSocketMessageType::BINARY) ==
                    net::WebSocketFrameHeader::kOpCodeBinary,
                "enum values must match for opcode binary");
  return static_cast<OpCode>(type);
}

mojom::WebSocketMessageType OpCodeToMessageType(
    net::WebSocketFrameHeader::OpCode opCode) {
  DCHECK(opCode == net::WebSocketFrameHeader::kOpCodeContinuation ||
         opCode == net::WebSocketFrameHeader::kOpCodeText ||
         opCode == net::WebSocketFrameHeader::kOpCodeBinary);
  // This cast is guaranteed valid by the static_assert() statements above.
  return static_cast<mojom::WebSocketMessageType>(opCode);
}

}  // namespace

// Implementation of net::WebSocketEventInterface. Receives events from our
// WebSocketChannel object.
class WebSocket::WebSocketEventHandler final
    : public net::WebSocketEventInterface {
 public:
  explicit WebSocketEventHandler(WebSocket* impl);
  ~WebSocketEventHandler() override;

  // net::WebSocketEventInterface implementation

  void OnCreateURLRequest(net::URLRequest* url_request) override;
  void OnAddChannelResponse(const std::string& selected_subprotocol,
                            const std::string& extensions) override;
  void OnDataFrame(bool fin,
                   WebSocketMessageType type,
                   scoped_refptr<net::IOBuffer> buffer,
                   size_t buffer_size) override;
  void OnClosingHandshake() override;
  void OnSendFlowControlQuotaAdded(int64_t quota) override;
  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const std::string& reason) override;
  void OnFailChannel(const std::string& message) override;
  void OnStartOpeningHandshake(
      std::unique_ptr<net::WebSocketHandshakeRequestInfo> request) override;
  void OnFinishOpeningHandshake(
      std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) override;
  void OnSSLCertificateError(
      std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks>
          callbacks,
      const GURL& url,
      int net_error,
      const net::SSLInfo& ssl_info,
      bool fatal) override;
  int OnAuthRequired(
      const net::AuthChallengeInfo& auth_info,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      const net::IPEndPoint& remote_endpoint,
      base::OnceCallback<void(const net::AuthCredentials*)> callback,
      base::Optional<net::AuthCredentials>* credentials) override;

 private:
  WebSocket* const impl_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketEventHandler);
};

WebSocket::WebSocketEventHandler::WebSocketEventHandler(WebSocket* impl)
    : impl_(impl) {
  DVLOG(1) << "WebSocketEventHandler created @"
           << reinterpret_cast<void*>(this);
}

WebSocket::WebSocketEventHandler::~WebSocketEventHandler() {
  DVLOG(1) << "WebSocketEventHandler destroyed @"
           << reinterpret_cast<void*>(this);
}

void WebSocket::WebSocketEventHandler::OnCreateURLRequest(
    net::URLRequest* url_request) {
  url_request->SetUserData(WebSocket::kUserDataKey,
                           std::make_unique<UnownedPointer>(impl_));
  impl_->delegate_->OnCreateURLRequest(impl_->child_id_, impl_->frame_id_,
                                       url_request);
}

void WebSocket::WebSocketEventHandler::OnAddChannelResponse(
    const std::string& selected_protocol,
    const std::string& extensions) {
  DVLOG(3) << "WebSocketEventHandler::OnAddChannelResponse @"
           << reinterpret_cast<void*>(this) << " selected_protocol=\""
           << selected_protocol << "\""
           << " extensions=\"" << extensions << "\"";

  impl_->handshake_succeeded_ = true;
  impl_->pending_connection_tracker_.OnCompleteHandshake();

  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  DCHECK(command_line);
  uint64_t receive_quota_threshold =
      net::WebSocketChannel::kReceiveQuotaThreshold;
  if (command_line->HasSwitch(net::kWebSocketReceiveQuotaThreshold)) {
    std::string flag_string =
        command_line->GetSwitchValueASCII(net::kWebSocketReceiveQuotaThreshold);
    if (!base::StringToUint64(flag_string, &receive_quota_threshold))
      receive_quota_threshold = net::WebSocketChannel::kReceiveQuotaThreshold;
  }
  DVLOG(3) << "receive_quota_threshold is " << receive_quota_threshold;
  impl_->handshake_client_->OnAddChannelResponse(selected_protocol, extensions,
                                                 receive_quota_threshold);
}

void WebSocket::WebSocketEventHandler::OnDataFrame(
    bool fin,
    net::WebSocketFrameHeader::OpCode type,
    scoped_refptr<net::IOBuffer> buffer,
    size_t buffer_size) {
  DVLOG(3) << "WebSocketEventHandler::OnDataFrame @"
           << reinterpret_cast<void*>(this) << " fin=" << fin
           << " type=" << type << " data is " << buffer_size << " bytes";

  // TODO(darin): Avoid this copy.
  std::vector<uint8_t> data_to_pass(buffer_size);
  if (buffer_size > 0) {
    std::copy(buffer->data(), buffer->data() + buffer_size,
              data_to_pass.begin());
  }

  impl_->client_->OnDataFrame(fin, OpCodeToMessageType(type), data_to_pass);
}

void WebSocket::WebSocketEventHandler::OnClosingHandshake() {
  DVLOG(3) << "WebSocketEventHandler::OnClosingHandshake @"
           << reinterpret_cast<void*>(this);

  impl_->client_->OnClosingHandshake();
}

void WebSocket::WebSocketEventHandler::OnSendFlowControlQuotaAdded(
    int64_t quota) {
  DVLOG(3) << "WebSocketEventHandler::OnSendFlowControlQuotaAdded @"
           << reinterpret_cast<void*>(this) << " quota=" << quota;

  impl_->client_->AddSendFlowControlQuota(quota);
}

void WebSocket::WebSocketEventHandler::OnDropChannel(
    bool was_clean,
    uint16_t code,
    const std::string& reason) {
  DVLOG(3) << "WebSocketEventHandler::OnDropChannel @"
           << reinterpret_cast<void*>(this) << " was_clean=" << was_clean
           << " code=" << code << " reason=\"" << reason << "\"";

  impl_->client_->OnDropChannel(was_clean, code, reason);

  // net::WebSocketChannel requires that we delete it at this point.
  impl_->channel_.reset();
}

void WebSocket::WebSocketEventHandler::OnFailChannel(
    const std::string& message) {
  DVLOG(3) << "WebSocketEventHandler::OnFailChannel @"
           << reinterpret_cast<void*>(this) << " message=\"" << message << "\"";

  impl_->client_->OnFailChannel(message);

  // net::WebSocketChannel requires that we delete it at this point.
  impl_->channel_.reset();
}

void WebSocket::WebSocketEventHandler::OnStartOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeRequestInfo> request) {
  bool can_read_raw_cookies = impl_->delegate_->CanReadRawCookies(request->url);

  DVLOG(3) << "WebSocketEventHandler::OnStartOpeningHandshake @"
           << reinterpret_cast<void*>(this)
           << " can_read_raw_cookies =" << can_read_raw_cookies;

  mojom::WebSocketHandshakeRequestPtr request_to_pass(
      mojom::WebSocketHandshakeRequest::New());
  request_to_pass->url.Swap(&request->url);
  std::string headers_text = base::StringPrintf(
      "GET %s HTTP/1.1\r\n", request_to_pass->url.spec().c_str());
  net::HttpRequestHeaders::Iterator it(request->headers);
  while (it.GetNext()) {
    if (!can_read_raw_cookies &&
        base::EqualsCaseInsensitiveASCII(it.name(),
                                         net::HttpRequestHeaders::kCookie)) {
      continue;
    }
    mojom::HttpHeaderPtr header(mojom::HttpHeader::New());
    header->name = it.name();
    header->value = it.value();
    request_to_pass->headers.push_back(std::move(header));
    headers_text.append(base::StringPrintf("%s: %s\r\n", it.name().c_str(),
                                           it.value().c_str()));
  }
  headers_text.append("\r\n");
  request_to_pass->headers_text = std::move(headers_text);

  impl_->handshake_client_->OnStartOpeningHandshake(std::move(request_to_pass));
}

void WebSocket::WebSocketEventHandler::OnFinishOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) {
  bool can_read_raw_cookies =
      impl_->delegate_->CanReadRawCookies(response->url);
  DVLOG(3) << "WebSocketEventHandler::OnFinishOpeningHandshake "
           << reinterpret_cast<void*>(this)
           << " CanReadRawCookies=" << can_read_raw_cookies;

  mojom::WebSocketHandshakeResponsePtr response_to_pass(
      mojom::WebSocketHandshakeResponse::New());
  response_to_pass->url.Swap(&response->url);
  response_to_pass->status_code = response->headers->response_code();
  response_to_pass->status_text = response->headers->GetStatusText();
  response_to_pass->http_version = response->headers->GetHttpVersion();
  response_to_pass->remote_endpoint = response->remote_endpoint;
  size_t iter = 0;
  std::string name, value;
  std::string headers_text =
      base::StrCat({response->headers->GetStatusLine(), "\r\n"});
  while (response->headers->EnumerateHeaderLines(&iter, &name, &value)) {
    if (can_read_raw_cookies ||
        !net::HttpResponseHeaders::IsCookieResponseHeader(name)) {
      // We drop cookie-related headers such as "set-cookie" when the
      // renderer doesn't have access.
      response_to_pass->headers.push_back(mojom::HttpHeader::New(name, value));
      base::StrAppend(&headers_text, {name, ": ", value, "\r\n"});
    }
  }
  headers_text.append("\r\n");
  response_to_pass->headers_text = headers_text;

  impl_->handshake_client_->OnFinishOpeningHandshake(
      std::move(response_to_pass));
}

void WebSocket::WebSocketEventHandler::OnSSLCertificateError(
    std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks> callbacks,
    const GURL& url,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DVLOG(3) << "WebSocketEventHandler::OnSSLCertificateError"
           << reinterpret_cast<void*>(this) << " url=" << url.spec()
           << " cert_status=" << ssl_info.cert_status << " fatal=" << fatal;
  impl_->delegate_->OnSSLCertificateError(std::move(callbacks), url,
                                          impl_->child_id_, impl_->frame_id_,
                                          net_error, ssl_info, fatal);
}

int WebSocket::WebSocketEventHandler::OnAuthRequired(
    const net::AuthChallengeInfo& auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    const net::IPEndPoint& remote_endpoint,
    base::OnceCallback<void(const net::AuthCredentials*)> callback,
    base::Optional<net::AuthCredentials>* credentials) {
  DVLOG(3) << "WebSocketEventHandler::OnAuthRequired"
           << reinterpret_cast<void*>(this);
  if (!impl_->auth_handler_) {
    *credentials = base::nullopt;
    return net::OK;
  }

  impl_->auth_handler_->OnAuthRequired(
      auth_info, std::move(response_headers), remote_endpoint,
      base::BindOnce(&WebSocket::OnAuthRequiredComplete,
                     impl_->weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
  return net::ERR_IO_PENDING;
}

WebSocket::WebSocket(
    std::unique_ptr<Delegate> delegate,
    mojom::WebSocketRequest request,
    mojom::AuthenticationHandlerPtr auth_handler,
    mojom::TrustedHeaderClientPtr header_client,
    WebSocketThrottler::PendingConnection pending_connection_tracker,
    int child_id,
    int frame_id,
    url::Origin origin,
    uint32_t options,
    base::TimeDelta delay)
    : delegate_(std::move(delegate)),
      binding_(this, std::move(request)),
      auth_handler_(std::move(auth_handler)),
      header_client_(std::move(header_client)),
      pending_connection_tracker_(std::move(pending_connection_tracker)),
      delay_(delay),
      pending_flow_control_quota_(0),
      options_(options),
      child_id_(child_id),
      frame_id_(frame_id),
      origin_(std::move(origin)),
      handshake_succeeded_(false),
      weak_ptr_factory_(this) {
  binding_.set_connection_error_handler(
      base::BindOnce(&WebSocket::OnConnectionError, base::Unretained(this)));

  if (header_client_) {
    // Make sure the request dies if |header_client_| has an error, otherwise
    // requests can hang.
    header_client_.set_connection_error_handler(
        base::BindOnce(&WebSocket::OnConnectionError, base::Unretained(this)));
  }
}

WebSocket::~WebSocket() {}

// static
const void* const WebSocket::kUserDataKey = &WebSocket::kUserDataKey;

void WebSocket::GoAway() {
  StartClosingHandshake(static_cast<uint16_t>(net::kWebSocketErrorGoingAway),
                        "");
}

void WebSocket::AddChannelRequest(
    const GURL& socket_url,
    const std::vector<std::string>& requested_protocols,
    const GURL& site_for_cookies,
    std::vector<mojom::HttpHeaderPtr> additional_headers,
    mojom::WebSocketHandshakeClientPtr handshake_client,
    mojom::WebSocketClientPtr client) {
  DVLOG(3) << "WebSocket::AddChannelRequest @" << reinterpret_cast<void*>(this)
           << " socket_url=\"" << socket_url << "\" requested_protocols=\""
           << base::JoinString(requested_protocols, ", ") << "\" origin=\""
           << origin_ << "\" site_for_cookies=\"" << site_for_cookies << "\"";

  if (client_ || !client) {
    delegate_->ReportBadMessage(
        Delegate::BadMessageReason::kUnexpectedAddChannelRequest, this);
    return;
  }

  handshake_client_ = std::move(handshake_client);
  client_ = std::move(client);

  DCHECK(!channel_);
  if (delay_ > base::TimeDelta()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WebSocket::AddChannel, weak_ptr_factory_.GetWeakPtr(),
                       socket_url, requested_protocols, site_for_cookies,
                       std::move(additional_headers)),
        delay_);
  } else {
    AddChannel(socket_url, requested_protocols, site_for_cookies,
               std::move(additional_headers));
  }
}

void WebSocket::SendFrame(bool fin,
                          mojom::WebSocketMessageType type,
                          const std::vector<uint8_t>& data) {
  DVLOG(3) << "WebSocket::SendFrame @" << reinterpret_cast<void*>(this)
           << " fin=" << fin << " type=" << type << " data is " << data.size()
           << " bytes";

  if (!handshake_succeeded_) {
    // The client should not be sending us frames until after we've informed
    // it that the channel has been opened (OnAddChannelResponse).
    delegate_->ReportBadMessage(
        Delegate::BadMessageReason::kUnexpectedSendFrame, this);
    return;
  }

  if (!channel_) {
    DVLOG(1) << "Dropping frame sent to closed websocket";
    return;
  }

  // This is guaranteed by the maximum size enforced on mojo messages.
  DCHECK_LE(data.size(), static_cast<size_t>(INT_MAX));

  // This is guaranteed by mojo.
  DCHECK(IsKnownEnumValue(type));

  // TODO(darin): Avoid this copy.
  auto data_to_pass = base::MakeRefCounted<net::IOBuffer>(data.size());
  std::copy(data.begin(), data.end(), data_to_pass->data());

  channel_->SendFrame(fin, MessageTypeToOpCode(type), std::move(data_to_pass),
                      data.size());
}

void WebSocket::AddReceiveFlowControlQuota(int64_t quota) {
  DVLOG(3) << "WebSocket::AddReceiveFlowControlQuota @"
           << reinterpret_cast<void*>(this) << " quota=" << quota;

  if (!channel_) {
    // WebSocketChannel is not yet created due to the delay introduced by
    // per-renderer WebSocket throttling.
    // SendFlowControl() is called after WebSocketChannel is created.
    pending_flow_control_quota_ += quota;
    return;
  }

  ignore_result(channel_->AddReceiveFlowControlQuota(quota));
}

void WebSocket::StartClosingHandshake(uint16_t code,
                                      const std::string& reason) {
  DVLOG(3) << "WebSocket::StartClosingHandshake @"
           << reinterpret_cast<void*>(this) << " code=" << code << " reason=\""
           << reason << "\"";

  if (!channel_) {
    // WebSocketChannel is not yet created due to the delay introduced by
    // per-renderer WebSocket throttling.
    if (client_) {
      client_->OnDropChannel(false, net::kWebSocketErrorAbnormalClosure, "");
    }
    return;
  }

  ignore_result(channel_->StartClosingHandshake(code, reason));
}

bool WebSocket::AllowCookies(const GURL& url) const {
  const GURL site_for_cookies = origin_.GetURL();
  net::StaticCookiePolicy::Type policy =
      net::StaticCookiePolicy::ALLOW_ALL_COOKIES;
  if (options_ & mojom::kWebSocketOptionBlockAllCookies) {
    policy = net::StaticCookiePolicy::BLOCK_ALL_COOKIES;
  } else if (options_ & mojom::kWebSocketOptionBlockThirdPartyCookies) {
    policy = net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES;
  } else {
    return true;
  }
  return net::StaticCookiePolicy(policy).CanAccessCookies(
             url, site_for_cookies) == net::OK;
}

int WebSocket::OnBeforeStartTransaction(net::CompletionOnceCallback callback,
                                        net::HttpRequestHeaders* headers) {
  if (header_client_) {
    header_client_->OnBeforeSendHeaders(
        *headers, base::BindOnce(&WebSocket::OnBeforeSendHeadersComplete,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(callback), headers));
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

int WebSocket::OnHeadersReceived(
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  if (header_client_) {
    header_client_->OnHeadersReceived(
        original_response_headers->raw_headers(),
        base::BindOnce(&WebSocket::OnHeadersReceivedComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       override_response_headers, allowed_unsafe_redirect_url));
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

// static
WebSocket* WebSocket::ForRequest(const net::URLRequest& request) {
  auto* pointer =
      static_cast<UnownedPointer*>(request.GetUserData(kUserDataKey));
  if (!pointer)
    return nullptr;
  return pointer->get();
}

void WebSocket::OnConnectionError() {
  DVLOG(3) << "WebSocket::OnConnectionError @" << reinterpret_cast<void*>(this);

  delegate_->OnLostConnectionToClient(this);
}

void WebSocket::AddChannel(
    const GURL& socket_url,
    const std::vector<std::string>& requested_protocols,
    const GURL& site_for_cookies,
    std::vector<mojom::HttpHeaderPtr> additional_headers) {
  DVLOG(3) << "WebSocket::AddChannel @" << reinterpret_cast<void*>(this)
           << " socket_url=\"" << socket_url << "\" requested_protocols=\""
           << base::JoinString(requested_protocols, ", ") << "\" origin=\""
           << origin_ << "\" site_for_cookies=\"" << site_for_cookies << "\"";

  DCHECK(!channel_);

  std::unique_ptr<net::WebSocketEventInterface> event_interface(
      new WebSocketEventHandler(this));
  channel_.reset(new net::WebSocketChannel(std::move(event_interface),
                                           delegate_->GetURLRequestContext()));

  int64_t quota = pending_flow_control_quota_;
  pending_flow_control_quota_ = 0;

  net::HttpRequestHeaders headers_to_pass;
  for (const auto& header : additional_headers) {
    if (net::HttpUtil::IsValidHeaderName(header->name) &&
        net::HttpUtil::IsValidHeaderValue(header->value) &&
        (net::HttpUtil::IsSafeHeader(header->name) ||
         base::EqualsCaseInsensitiveASCII(
             header->name, net::HttpRequestHeaders::kUserAgent) ||
         base::EqualsCaseInsensitiveASCII(header->name,
                                          net::HttpRequestHeaders::kCookie) ||
         base::EqualsCaseInsensitiveASCII(header->name, "cookie2"))) {
      headers_to_pass.SetHeader(header->name, header->value);
    }
  }
  channel_->SendAddChannelRequest(socket_url, requested_protocols, origin_,
                                  site_for_cookies, headers_to_pass);
  if (quota > 0)
    AddReceiveFlowControlQuota(quota);
}

void WebSocket::OnAuthRequiredComplete(
    base::OnceCallback<void(const net::AuthCredentials*)> callback,
    const base::Optional<net::AuthCredentials>& credentials) {
  DCHECK(!handshake_succeeded_);
  if (!channel_) {
    // Something happened before the authentication response arrives.
    return;
  }

  std::move(callback).Run(credentials ? &*credentials : nullptr);
}

void WebSocket::OnBeforeSendHeadersComplete(
    net::CompletionOnceCallback callback,
    net::HttpRequestHeaders* out_headers,
    int result,
    const base::Optional<net::HttpRequestHeaders>& headers) {
  if (!channel_) {
    // Something happened before the OnBeforeSendHeaders response arrives.
    return;
  }
  if (headers)
    *out_headers = headers.value();
  std::move(callback).Run(result);
}

void WebSocket::OnHeadersReceivedComplete(
    net::CompletionOnceCallback callback,
    scoped_refptr<net::HttpResponseHeaders>* out_headers,
    GURL* out_allowed_unsafe_redirect_url,
    int result,
    const base::Optional<std::string>& headers,
    const GURL& allowed_unsafe_redirect_url) {
  if (!channel_) {
    // Something happened before the OnHeadersReceived response arrives.
    return;
  }
  if (headers) {
    *out_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(headers.value());
  }
  std::move(callback).Run(result);
}

}  // namespace network
