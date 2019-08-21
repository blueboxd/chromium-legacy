// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation/browser_presentation_connection_proxy.h"

#include <memory>

#include "base/run_loop.h"
#include "chrome/browser/media/router/route_message_util.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/media_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"

using blink::mojom::PresentationConnectionMessage;
using blink::mojom::PresentationConnectionMessagePtr;
using media_router::mojom::RouteMessagePtr;
using ::testing::_;
using ::testing::Invoke;

namespace media_router {

namespace {

void ExpectMessage(const PresentationConnectionMessagePtr expected_message,
                   const PresentationConnectionMessagePtr message) {
  EXPECT_EQ(expected_message, message);
}

}  // namespace

constexpr char kMediaRouteId[] = "MockRouteId";

class BrowserPresentationConnectionProxyTest : public ::testing::Test {
 public:
  BrowserPresentationConnectionProxyTest() = default;

  void SetUp() override {
    mock_controller_connection_proxy_ =
        std::make_unique<MockPresentationConnectionProxy>();
    blink::mojom::PresentationConnectionPtr controller_connection_ptr;
    binding_.reset(new mojo::Binding<blink::mojom::PresentationConnection>(
        mock_controller_connection_proxy_.get(),
        mojo::MakeRequest(&controller_connection_ptr)));
    EXPECT_CALL(mock_router_, RegisterRouteMessageObserver(_));
    EXPECT_CALL(
        *mock_controller_connection_proxy_,
        DidChangeState(blink::mojom::PresentationConnectionState::CONNECTED));

    blink::mojom::PresentationConnectionPtr receiver_connection_ptr;

    base::RunLoop run_loop;
    browser_connection_proxy_ =
        std::make_unique<BrowserPresentationConnectionProxy>(
            &mock_router_, "MockRouteId",
            mojo::MakeRequest(&receiver_connection_ptr),
            std::move(controller_connection_ptr));
    run_loop.RunUntilIdle();
  }

  void TearDown() override {
    EXPECT_CALL(mock_router_, UnregisterRouteMessageObserver(_));
    browser_connection_proxy_.reset();
    binding_.reset();
    mock_controller_connection_proxy_.reset();
  }

  MockPresentationConnectionProxy* controller_connection_proxy() {
    return mock_controller_connection_proxy_.get();
  }

  BrowserPresentationConnectionProxy* browser_connection_proxy() {
    return browser_connection_proxy_.get();
  }

  MockMediaRouter* mock_router() { return &mock_router_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<MockPresentationConnectionProxy>
      mock_controller_connection_proxy_;
  std::unique_ptr<mojo::Binding<blink::mojom::PresentationConnection>> binding_;
  std::unique_ptr<BrowserPresentationConnectionProxy> browser_connection_proxy_;
  MockMediaRouter mock_router_;
};

TEST_F(BrowserPresentationConnectionProxyTest, TestOnMessageTextMessage) {
  std::string message = "test message";
  PresentationConnectionMessagePtr connection_message =
      PresentationConnectionMessage::NewMessage(message);

  EXPECT_CALL(*mock_router(), SendRouteMessage(kMediaRouteId, message));

  browser_connection_proxy()->OnMessage(std::move(connection_message));
}

TEST_F(BrowserPresentationConnectionProxyTest, TestOnMessageBinaryMessage) {
  std::vector<uint8_t> expected_data;
  expected_data.push_back(42);
  expected_data.push_back(36);

  PresentationConnectionMessagePtr connection_message =
      PresentationConnectionMessage::NewData(expected_data);

  EXPECT_CALL(*mock_router(), SendRouteBinaryMessage(_, _))
      .WillOnce([&expected_data](const MediaRoute::Id& route_id,
                                 std::unique_ptr<std::vector<uint8_t>> data) {
        EXPECT_EQ(*data, expected_data);
      });

  browser_connection_proxy()->OnMessage(std::move(connection_message));
}

TEST_F(BrowserPresentationConnectionProxyTest, OnMessagesReceived) {
  EXPECT_CALL(*controller_connection_proxy(), OnMessage(_))
      .WillOnce([&](auto message) {
        ExpectMessage(PresentationConnectionMessage::NewMessage("foo"),
                      std::move(message));
      })
      .WillOnce([&](auto message) {
        ExpectMessage(PresentationConnectionMessage::NewData(
                          std::vector<uint8_t>({1, 2, 3})),
                      std::move(message));
      });

  std::vector<RouteMessagePtr> route_messages;
  route_messages.emplace_back(message_util::RouteMessageFromString("foo"));
  route_messages.emplace_back(
      message_util::RouteMessageFromData(std::vector<uint8_t>({1, 2, 3})));
  browser_connection_proxy()->OnMessagesReceived(std::move(route_messages));
  base::RunLoop().RunUntilIdle();
}

}  // namespace media_router
