// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/websockets/websocket_manager.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/post_task.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/browser/websockets/websocket_handshake_request_info_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/features.h"

namespace content {

namespace {

const char kWebSocketManagerKeyName[] = "web_socket_manager";

}  // namespace

class WebSocketManager::Delegate final : public network::WebSocket::Delegate {
 public:
  explicit Delegate(WebSocketManager* manager) : manager_(manager) {}
  ~Delegate() override {}

  net::URLRequestContext* GetURLRequestContext() override {
    return manager_->GetURLRequestContext();
  }

  void OnLostConnectionToClient(network::WebSocket* impl) override {
    manager_->OnLostConnectionToClient(impl);
  }

  void OnSSLCertificateError(
      std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks>
          callbacks,
      const GURL& url,
      int child_id,
      int frame_id,
      int net_error,
      const net::SSLInfo& ssl_info,
      bool fatal) override {
    ssl_error_handler_delegate_ =
        std::make_unique<SSLErrorHandlerDelegate>(std::move(callbacks));
    SSLManager::OnSSLCertificateSubresourceError(
        ssl_error_handler_delegate_->GetWeakPtr(), url, child_id, frame_id,
        net_error, ssl_info, fatal);
  }

  void ReportBadMessage(BadMessageReason reason,
                        network::WebSocket* impl) override {
    bad_message::BadMessageReason reason_to_pass =
        bad_message::WSI_UNEXPECTED_ADD_CHANNEL_REQUEST;
    switch (reason) {
      case BadMessageReason::kUnexpectedAddChannelRequest:
        reason_to_pass = bad_message::WSI_UNEXPECTED_ADD_CHANNEL_REQUEST;
        break;
      case BadMessageReason::kUnexpectedSendFrame:
        reason_to_pass = bad_message::WSI_UNEXPECTED_SEND_FRAME;
        break;
    }
    bad_message::ReceivedBadMessage(manager_->process_id_, reason_to_pass);
    OnLostConnectionToClient(impl);
  }

  bool CanReadRawCookies(const GURL& url) override {
    ChildProcessSecurityPolicyImpl* impl =
        ChildProcessSecurityPolicyImpl::GetInstance();
    return impl->CanReadRawCookies(manager_->process_id_) &&
           impl->CanAccessDataForWebSocket(manager_->process_id_, url);
  }

  void OnCreateURLRequest(int child_id,
                          int frame_id,
                          net::URLRequest* url_request) override {
    WebSocketHandshakeRequestInfoImpl::CreateInfoAndAssociateWithRequest(
        child_id, frame_id, url_request);
  }

 private:
  class SSLErrorHandlerDelegate final : public SSLErrorHandler::Delegate {
   public:
    explicit SSLErrorHandlerDelegate(
        std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks>
            callbacks)
        : callbacks_(std::move(callbacks)) {}
    ~SSLErrorHandlerDelegate() override {}

    base::WeakPtr<SSLErrorHandler::Delegate> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

    // SSLErrorHandler::Delegate methods
    void CancelSSLRequest(int error, const net::SSLInfo* ssl_info) override {
      DVLOG(3) << "SSLErrorHandlerDelegate::CancelSSLRequest"
               << " error=" << error << " cert_status="
               << (ssl_info ? ssl_info->cert_status
                            : static_cast<net::CertStatus>(-1));
      callbacks_->CancelSSLRequest(error, ssl_info);
    }

    void ContinueSSLRequest() override {
      DVLOG(3) << "SSLErrorHandlerDelegate::ContinueSSLRequest";
      callbacks_->ContinueSSLRequest();
    }

   private:
    std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks> callbacks_;

    base::WeakPtrFactory<SSLErrorHandlerDelegate> weak_ptr_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(SSLErrorHandlerDelegate);
  };

  std::unique_ptr<SSLErrorHandlerDelegate> ssl_error_handler_delegate_;
  // |manager_| outlives this object.
  WebSocketManager* const manager_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

class WebSocketManager::Handle : public base::SupportsUserData::Data,
                                 public RenderProcessHostObserver {
 public:
  explicit Handle(WebSocketManager* manager) : manager_(manager) {}

  ~Handle() override {
    DCHECK(!manager_) << "Should have received RenderProcessHostDestroyed";
  }

  WebSocketManager* manager() const { return manager_; }

  // The network stack could be shutdown after this notification, so be sure to
  // stop using it before then.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, manager_);
    manager_ = nullptr;
  }

 private:
  WebSocketManager* manager_;
};

// static
void WebSocketManager::CreateWebSocket(
    const GURL& url,
    const std::vector<std::string>& requested_protocols,
    const GURL& site_for_cookies,
    const base::Optional<std::string>& user_agent,
    RenderProcessHost* process,
    int32_t frame_id,
    const url::Origin& origin,
    uint32_t options,
    network::mojom::WebSocketHandshakeClientPtr handshake_client,
    network::mojom::WebSocketClientPtr websocket_client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  StoragePartition* storage_partition = process->GetStoragePartition();
  const int process_id = process->GetID();

  // Maintain a WebSocketManager per RenderProcessHost. While the instance
  // of WebSocketManager is allocated on the UI thread, it must only be used
  // and deleted from the IO thread.
  Handle* handle =
      static_cast<Handle*>(process->GetUserData(kWebSocketManagerKeyName));
  if (!handle) {
    handle = new Handle(new WebSocketManager(process_id, storage_partition));
    process->SetUserData(kWebSocketManagerKeyName, base::WrapUnique(handle));
    process->AddObserver(handle);
  }
  DCHECK(handle->manager());

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &WebSocketManager::DoCreateWebSocket,
          base::Unretained(handle->manager()), url, requested_protocols,
          site_for_cookies, user_agent, frame_id, origin, options,
          handshake_client.PassInterface(), websocket_client.PassInterface()));
}

WebSocketManager::WebSocketManager(int process_id,
                                   StoragePartition* storage_partition)
    : process_id_(process_id),
      context_destroyed_(false) {
  if (storage_partition) {
    url_request_context_getter_ = storage_partition->GetURLRequestContext();
    // This unretained pointer is safe because we destruct a WebSocketManager
    // only via WebSocketManager::Handle::RenderProcessHostDestroyed which
    // posts a deletion task to the IO thread.
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&WebSocketManager::ObserveURLRequestContextGetter,
                       base::Unretained(this)));
  }
}

WebSocketManager::~WebSocketManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!context_destroyed_ && url_request_context_getter_)
    url_request_context_getter_->RemoveObserver(this);

  for (const auto& impl : impls_) {
    impl->GoAway();
  }
}

void WebSocketManager::DoCreateWebSocket(
    const GURL& url,
    const std::vector<std::string>& requested_protocols,
    const GURL& site_for_cookies,
    const base::Optional<std::string>& user_agent,
    int32_t frame_id,
    const url::Origin& origin,
    uint32_t options,
    network::mojom::WebSocketHandshakeClientPtrInfo handshake_client_info,
    network::mojom::WebSocketClientPtrInfo websocket_client_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  auto handshake_client = mojo::MakeProxy(std::move(handshake_client_info));
  auto websocket_client = mojo::MakeProxy(std::move(websocket_client_info));

  if (throttler_.HasTooManyPendingConnections()) {
    // Too many websockets!
    handshake_client.ResetWithReason(
        network::mojom::WebSocket::kInsufficientResources,
        "Error in connection establishment: net::ERR_INSUFFICIENT_RESOURCES");
    return;
  }

  if (context_destroyed_) {
    handshake_client.ResetWithReason(
        network::mojom::WebSocket::kInsufficientResources,
        "Error in connection establishment: net::ERR_UNEXPECTED");
    return;
  }
  // Keep all network::WebSockets alive until either the client drops its
  // connection (see OnLostConnectionToClient) or we need to shutdown.
  impls_.insert(DoCreateWebSocketInternal(
      std::make_unique<Delegate>(this), url, requested_protocols,
      site_for_cookies, user_agent, frame_id, origin, options,
      std::move(handshake_client), std::move(websocket_client),
      throttler_.IssuePendingConnectionTracker(), throttler_.CalculateDelay()));

  if (!throttling_period_timer_.IsRunning()) {
    throttling_period_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMinutes(2),
        this,
        &WebSocketManager::ThrottlingPeriodTimerCallback);
  }
}

void WebSocketManager::ThrottlingPeriodTimerCallback() {
  throttler_.Roll();
  if (throttler_.IsClean())
    throttling_period_timer_.Stop();
}

std::unique_ptr<network::WebSocket> WebSocketManager::DoCreateWebSocketInternal(
    std::unique_ptr<network::WebSocket::Delegate> delegate,
    const GURL& url,
    const std::vector<std::string>& requested_protocols,
    const GURL& site_for_cookies,
    const base::Optional<std::string>& user_agent,
    int32_t frame_id,
    const url::Origin& origin,
    uint32_t options,
    network::mojom::WebSocketHandshakeClientPtr handshake_client,
    network::mojom::WebSocketClientPtr websocket_client,
    network::WebSocketThrottler::PendingConnection pending_connection_tracker,
    base::TimeDelta delay) {
  std::vector<network::mojom::HttpHeaderPtr> headers;
  if (user_agent) {
    headers.push_back(network::mojom::HttpHeader::New(
        net::HttpRequestHeaders::kUserAgent, *user_agent));
  }
  return std::make_unique<network::WebSocket>(
      std::move(delegate), url, requested_protocols, site_for_cookies,
      std::move(headers), process_id_, frame_id, origin, options,
      std::move(handshake_client), std::move(websocket_client), nullptr,
      nullptr, std::move(pending_connection_tracker), delay);
}

net::URLRequestContext* WebSocketManager::GetURLRequestContext() {
  return url_request_context_getter_->GetURLRequestContext();
}

void WebSocketManager::OnLostConnectionToClient(network::WebSocket* impl) {
  // The client is no longer interested in this WebSocket.
  impl->GoAway();
  const auto it = impls_.find(impl);
  DCHECK(it != impls_.end());
  impls_.erase(it);
}

void WebSocketManager::OnContextShuttingDown() {
  context_destroyed_ = true;
  url_request_context_getter_ = nullptr;
  for (const auto& impl : impls_) {
    impl->GoAway();
  }
  impls_.clear();
}

void WebSocketManager::ObserveURLRequestContextGetter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!url_request_context_getter_->GetURLRequestContext()) {
    context_destroyed_ = true;
    url_request_context_getter_ = nullptr;
    return;
  }
  url_request_context_getter_->AddObserver(this);
}

}  // namespace content
