// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/media_session/assistant_media_session.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chromeos/services/assistant/fake_assistant_manager_service_impl.h"
#include "chromeos/services/assistant/test_support/scoped_assistant_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

using media_session::mojom::MediaSession;
using media_session::mojom::MediaSessionAction;
using media_session::mojom::MediaSessionInfo;

class AssistantMediaSessionTest : public testing::Test {
 public:
  AssistantMediaSessionTest()
      : assistant_media_session_(std::make_unique<AssistantMediaSession>(
            &fake_assistant_manager_service_impl_)) {}
  ~AssistantMediaSessionTest() override = default;

  AssistantMediaSession* assistant_media_session() {
    return assistant_media_session_.get();
  }

  FakeAssistantManagerServiceImpl* assistant_manager_service() {
    return &fake_assistant_manager_service_impl_;
  }

 private:
  // Needed for a test environment to support post tasks and should outlive
  // other class members.
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<AssistantMediaSession> assistant_media_session_;
  ScopedAssistantClient fake_client_;
  FakeAssistantManagerServiceImpl fake_assistant_manager_service_impl_;
};

TEST_F(AssistantMediaSessionTest, ShouldUpdateSessionStateOnStartStopDucking) {
  assistant_media_session()->StartDucking();
  EXPECT_TRUE(assistant_media_session()->IsSessionStateDucking());

  assistant_media_session()->StopDucking();
  EXPECT_TRUE(assistant_media_session()->IsSessionStateActive());
}

TEST_F(AssistantMediaSessionTest,
       ShouldUpdateSessionStateAndSendActionOnSuspendResumePlaying) {
  assistant_media_session()->Suspend(MediaSession::SuspendType::kSystem);
  EXPECT_TRUE(assistant_media_session()->IsSessionStateSuspended());
  EXPECT_EQ(assistant_manager_service()->media_session_action(),
            MediaSessionAction::kPause);

  assistant_media_session()->Resume(MediaSession::SuspendType::kSystem);
  EXPECT_TRUE(assistant_media_session()->IsSessionStateActive());
  EXPECT_EQ(assistant_manager_service()->media_session_action(),
            MediaSessionAction::kPlay);

  assistant_media_session()->Suspend(MediaSession::SuspendType::kSystem);
  EXPECT_TRUE(assistant_media_session()->IsSessionStateSuspended());
  EXPECT_EQ(assistant_manager_service()->media_session_action(),
            MediaSessionAction::kPause);
}

}  // namespace assistant
}  // namespace chromeos
