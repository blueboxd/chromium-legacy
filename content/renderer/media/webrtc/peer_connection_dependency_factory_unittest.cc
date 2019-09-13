// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/mock_web_rtc_peer_connection_handler_client.h"
#include "content/renderer/media/webrtc/rtc_peer_connection_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

namespace content {

// blink::Platform implementation that overrides the known method needed
// by the test: CreateRTCPeerConnectionHandler().
//
// TODO(crbug.com/787254): When this file moves to blink/renderer/, the
// implementation of
// PeerConnectionDependencyFactory::CreateRTCPeerConnectionHandler will not
// route through Platform anymore, and this custom implementation below ain't
// going to be needed.
class PeerConnectionDependencyFactoryTestingPlatformSupport
    : public blink::Platform {
 public:
  PeerConnectionDependencyFactoryTestingPlatformSupport(
      MockPeerConnectionDependencyFactory* dependency_factory)
      : dependency_factory_(dependency_factory) {}

  std::unique_ptr<blink::WebRTCPeerConnectionHandler>
  CreateRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    return std::make_unique<RTCPeerConnectionHandler>(
        client, dependency_factory_, task_runner);
  }

 private:
  MockPeerConnectionDependencyFactory* dependency_factory_ = nullptr;
};

class PeerConnectionDependencyFactoryTest : public ::testing::Test {
 public:
  void SetUp() override {
    dependency_factory_.reset(new MockPeerConnectionDependencyFactory());

    platform_original_ = blink::Platform::Current();
    peer_connection_dependency_factory_platform_support_.reset(
        new PeerConnectionDependencyFactoryTestingPlatformSupport(
            dependency_factory_.get()));
    blink::Platform::SetCurrentPlatformForTesting(
        peer_connection_dependency_factory_platform_support_.get());
  }

  void TearDown() override {
    blink::Platform::SetCurrentPlatformForTesting(platform_original_);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<MockPeerConnectionDependencyFactory> dependency_factory_;

  std::unique_ptr<PeerConnectionDependencyFactoryTestingPlatformSupport>
      peer_connection_dependency_factory_platform_support_;
  blink::Platform* platform_original_ = nullptr;
};

TEST_F(PeerConnectionDependencyFactoryTest, CreateRTCPeerConnectionHandler) {
  MockWebRTCPeerConnectionHandlerClient client_jsep;
  std::unique_ptr<blink::WebRTCPeerConnectionHandler> pc_handler(
      dependency_factory_->CreateRTCPeerConnectionHandler(
          &client_jsep,
          blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
  EXPECT_TRUE(pc_handler.get() != nullptr);
}

}  // namespace content
