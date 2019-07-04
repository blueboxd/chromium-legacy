// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <vector>

#include "content/browser/websockets/websocket_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

// This number is unlikely to occur by chance.
static const int kMagicRenderProcessId = 506116062;

class TestWebSocketImpl : public network::WebSocket {
 public:
  TestWebSocketImpl(
      std::unique_ptr<Delegate> delegate,
      network::mojom::WebSocketHandshakeClientPtr handshake_client,
      network::mojom::WebSocketClientPtr websocket_client,
      network::WebSocketThrottler::PendingConnection pending_connection_tracker)
      : network::WebSocket(std::move(delegate),
                           GURL(),
                           std::vector<std::string>(),
                           GURL(),
                           std::vector<network::mojom::HttpHeaderPtr>(),
                           kMagicRenderProcessId,
                           MSG_ROUTING_NONE,
                           url::Origin(),
                           network::mojom::kWebSocketOptionNone,
                           std::move(handshake_client),
                           std::move(websocket_client),
                           nullptr,
                           nullptr,
                           std::move(pending_connection_tracker),
                           base::TimeDelta::FromSeconds(1)) {}
};

class TestWebSocketManager : public WebSocketManager {
 public:
  TestWebSocketManager()
      : WebSocketManager(kMagicRenderProcessId, nullptr) {}

  int64_t num_pending_connections() const {
    return throttler_.num_pending_connections();
  }
  int64_t num_current_succeeded_connections() const {
    return throttler_.num_current_succeeded_connections();
  }
  int64_t num_previous_succeeded_connections() const {
    return throttler_.num_previous_succeeded_connections();
  }
  int64_t num_current_failed_connections() const {
    return throttler_.num_current_failed_connections();
  }
  int64_t num_previous_failed_connections() const {
    return throttler_.num_previous_failed_connections();
  }

  void DoCreateWebSocket() {
    network::mojom::WebSocketHandshakeClientPtr handshake_client;
    network::mojom::WebSocketClientPtr websocket_client;

    handshake_client_requests_.push_back(mojo::MakeRequest(&handshake_client));
    websocket_client_requests_.push_back(mojo::MakeRequest(&websocket_client));

    WebSocketManager::DoCreateWebSocket(
        GURL(), std::vector<std::string>(), GURL(), base::nullopt,
        MSG_ROUTING_NONE, url::Origin(), network::mojom::kWebSocketOptionNone,
        handshake_client.PassInterface(), websocket_client.PassInterface());
  }

 private:
  std::unique_ptr<network::WebSocket> DoCreateWebSocketInternal(
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
      base::TimeDelta delay) override {
    return std::make_unique<TestWebSocketImpl>(
        std::move(delegate), std::move(handshake_client),
        std::move(websocket_client), std::move(pending_connection_tracker));
  }

  std::vector<network::mojom::WebSocketHandshakeClientRequest>
      handshake_client_requests_;
  std::vector<network::mojom::WebSocketClientRequest>
      websocket_client_requests_;
};

class WebSocketManagerTest : public ::testing::Test {
 public:
  WebSocketManagerTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {
    websocket_manager_.reset(new TestWebSocketManager());
  }

  TestWebSocketManager* websocket_manager() { return websocket_manager_.get(); }

 private:
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestWebSocketManager> websocket_manager_;
};

TEST_F(WebSocketManagerTest, Construct) {
  // Do nothing.
}

TEST_F(WebSocketManagerTest, CreateWebSocket) {
  network::mojom::WebSocketPtr websocket;

  websocket_manager()->DoCreateWebSocket();

  EXPECT_EQ(1U, websocket_manager()->num_sockets());
}

// The 256th connection is rejected by per-renderer WebSocket throttling.
// This is not counted as a failure.
TEST_F(WebSocketManagerTest, Rejects256thPendingConnection) {
  for (int i = 0; i < 256; ++i) {
    websocket_manager()->DoCreateWebSocket();
  }

  EXPECT_EQ(255, websocket_manager()->num_pending_connections());
  EXPECT_EQ(0, websocket_manager()->num_current_succeeded_connections());
  EXPECT_EQ(0, websocket_manager()->num_previous_succeeded_connections());
  EXPECT_EQ(0, websocket_manager()->num_current_failed_connections());
  EXPECT_EQ(0, websocket_manager()->num_previous_failed_connections());

  EXPECT_EQ(255U, websocket_manager()->num_sockets());
}

}  // namespace
}  // namespace content
