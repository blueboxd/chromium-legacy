// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most testing for WebSocketStream is done via web platform tests. These unit
// tests just cover the most common functionality.

#include "third_party/blink/renderer/modules/websockets/websocket_stream.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/websockets/mock_websocket_channel.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel_client.h"
#include "third_party/blink/renderer/modules/websockets/websocket_close_info.h"
#include "third_party/blink/renderer/modules/websockets/websocket_stream_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"

namespace blink {

namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

typedef testing::StrictMock<testing::MockFunction<void(int)>>
    Checkpoint;  // NOLINT

class WebSocketStreamTest : public ::testing::Test {
 public:
  WebSocketStreamTest() : channel_(MockWebSocketChannel::Create()) {}

  void TearDown() override {
    testing::Mock::VerifyAndClear(channel_);
    channel_ = nullptr;
  }

  // Returns a reference for easy use with EXPECT_CALL(Channel(), ...).
  MockWebSocketChannel& Channel() const { return *channel_; }

  WebSocketStream* Create(ScriptState* script_state,
                          const String& url,
                          ExceptionState& exception_state) {
    return Create(script_state, url, WebSocketStreamOptions::Create(),
                  exception_state);
  }

  WebSocketStream* Create(ScriptState* script_state,
                          const String& url,
                          WebSocketStreamOptions* options,
                          ExceptionState& exception_state) {
    return WebSocketStream::CreateForTesting(script_state, url, options,
                                             channel_, exception_state);
  }

 private:
  Persistent<MockWebSocketChannel> channel_;
};

TEST_F(WebSocketStreamTest, ConstructWithBadURL) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();

  EXPECT_CALL(Channel(), ApplyBackpressure());

  auto* stream = Create(scope.GetScriptState(), "bad-scheme:", exception_state);

  EXPECT_FALSE(stream);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(DOMExceptionCode::kSyntaxError,
            exception_state.CodeAs<DOMExceptionCode>());
  EXPECT_EQ(
      "The URL's scheme must be either 'ws' or 'wss'. 'bad-scheme' is not "
      "allowed.",
      exception_state.Message());
}

// Most coverage for bad constructor arguments is provided by
// dom_websocket_test.cc.
// TODO(ricea): Should we duplicate those tests here?

TEST_F(WebSocketStreamTest, Connect) {
  V8TestingScope scope;

  {
    InSequence s;
    EXPECT_CALL(Channel(), ApplyBackpressure());
    EXPECT_CALL(Channel(), Connect(KURL("ws://example.com/hoge"), String()))
        .WillOnce(Return(true));
  }

  auto* stream = Create(scope.GetScriptState(), "ws://example.com/hoge",
                        ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(stream);
  EXPECT_EQ(KURL("ws://example.com/hoge"), stream->url());
}

TEST_F(WebSocketStreamTest, ConnectWithProtocols) {
  V8TestingScope scope;

  {
    InSequence s;
    EXPECT_CALL(Channel(), ApplyBackpressure());
    EXPECT_CALL(Channel(),
                Connect(KURL("ws://example.com/chat"), String("chat0, chat1")))
        .WillOnce(Return(true));
  }

  auto* options = WebSocketStreamOptions::Create();
  options->setProtocols({"chat0", "chat1"});
  auto* stream = Create(scope.GetScriptState(), "ws://example.com/chat",
                        options, ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(stream);
  EXPECT_EQ(KURL("ws://example.com/chat"), stream->url());
}

TEST_F(WebSocketStreamTest, ConnectWithFailedHandshake) {
  V8TestingScope scope;

  {
    InSequence s;
    EXPECT_CALL(Channel(), ApplyBackpressure());
    EXPECT_CALL(Channel(), Connect(KURL("ws://example.com/chat"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(Channel(), Disconnect());
  }

  auto* stream = Create(scope.GetScriptState(), "ws://example.com/chat",
                        ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(stream);
  EXPECT_EQ(KURL("ws://example.com/chat"), stream->url());

  stream->DidError();
  stream->DidClose(WebSocketChannelClient::kClosingHandshakeIncomplete,
                   WebSocketChannel::kCloseEventCodeAbnormalClosure, String());

  // TODO(ricea): Verify the promises are rejected correctly.
}

TEST_F(WebSocketStreamTest, ConnectWithSuccessfulHandshake) {
  V8TestingScope scope;
  Checkpoint checkpoint;

  {
    InSequence s;
    EXPECT_CALL(Channel(), ApplyBackpressure());
    EXPECT_CALL(Channel(),
                Connect(KURL("ws://example.com/chat"), String("chat")))
        .WillOnce(Return(true));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(Channel(), Close(1001, String()));
  }

  auto* options = WebSocketStreamOptions::Create();
  options->setProtocols({"chat"});
  auto* stream = Create(scope.GetScriptState(), "ws://example.com/chat",
                        options, ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(stream);
  EXPECT_EQ(KURL("ws://example.com/chat"), stream->url());

  stream->DidConnect("chat", "permessage-deflate");

  // TODO(ricea): Verify the connection promise is resolved correctly.

  // Destruction of V8TestingScope causes Close() to be called.
  checkpoint.Call(1);
}

TEST_F(WebSocketStreamTest, ConnectThenCloseCleanly) {
  V8TestingScope scope;

  {
    InSequence s;
    EXPECT_CALL(Channel(), ApplyBackpressure());
    EXPECT_CALL(Channel(), Connect(KURL("ws://example.com/echo"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(Channel(), Close(-1, String("")));
    EXPECT_CALL(Channel(), Disconnect());
  }

  auto* stream = Create(scope.GetScriptState(), "ws://example.com/echo",
                        ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(stream);

  stream->DidConnect("", "");
  stream->close(MakeGarbageCollected<WebSocketCloseInfo>(),
                scope.GetExceptionState());
  stream->DidClose(WebSocketChannelClient::kClosingHandshakeComplete, 1005, "");
}

TEST_F(WebSocketStreamTest, CloseDuringHandshake) {
  V8TestingScope scope;

  {
    InSequence s;
    EXPECT_CALL(Channel(), ApplyBackpressure());
    EXPECT_CALL(Channel(), Connect(KURL("ws://example.com/echo"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(
        Channel(),
        FailMock(
            String("WebSocket is closed before the connection is established."),
            mojom::ConsoleMessageLevel::kWarning, _));
    EXPECT_CALL(Channel(), Disconnect());
  }

  auto* stream = Create(scope.GetScriptState(), "ws://example.com/echo",
                        ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(stream);

  stream->close(MakeGarbageCollected<WebSocketCloseInfo>(),
                scope.GetExceptionState());
  stream->DidClose(WebSocketChannelClient::kClosingHandshakeIncomplete, 1006,
                   "");
}

}  // namespace

}  // namespace blink
