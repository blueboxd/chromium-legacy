// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_dialog_controller.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/global_media_controls/media_dialog_controller_delegate.h"
#include "components/media_message_center/media_notification_item.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media_session::mojom::AudioFocusRequestState;
using media_session::mojom::AudioFocusRequestStatePtr;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;
using testing::_;

namespace {

class MockMediaDialogControllerDelegate : public MediaDialogControllerDelegate {
 public:
  MockMediaDialogControllerDelegate() = default;
  ~MockMediaDialogControllerDelegate() override = default;

  // MediaDialogControllerDelegate implementation.
  MOCK_METHOD2(
      ShowMediaSession,
      void(const std::string& id,
           base::WeakPtr<media_message_center::MediaNotificationItem> item));
  MOCK_METHOD1(HideMediaSession, void(const std::string& id));
};

}  // anonymous namespace

class MediaDialogControllerTest : public testing::Test {
 public:
  MediaDialogControllerTest() = default;
  ~MediaDialogControllerTest() override = default;

  void SetUp() override {
    controller_ = std::make_unique<MediaDialogController>(nullptr, &delegate_);
  }

 protected:
  void SimulateFocusGained(const base::UnguessableToken& id,
                           bool controllable) {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->is_controllable = controllable;

    AudioFocusRequestStatePtr focus(AudioFocusRequestState::New());
    focus->request_id = id;
    focus->session_info = std::move(session_info);
    controller_->OnFocusGained(std::move(focus));
  }

  void SimulateFocusLost(const base::UnguessableToken& id) {
    AudioFocusRequestStatePtr focus(AudioFocusRequestState::New());
    focus->request_id = id;
    controller_->OnFocusLost(std::move(focus));
  }

  void SimulateNecessaryMetadata(const base::UnguessableToken& id) {
    // In order for the MediaNotificationItem to tell the MediaDialogController
    // to show a media session, that session needs a title and artist. Typically
    // this would happen through the media session service, but since the
    // service doesn't run for this test, we'll manually grab the
    // MediaNotificationItem from the MediaDialogController and set the
    // metadata.
    auto item_itr = controller_->sessions_.find(id.ToString());
    ASSERT_NE(controller_->sessions_.end(), item_itr);

    media_session::MediaMetadata metadata;
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    item_itr->second.MediaSessionMetadataChanged(std::move(metadata));
  }

  MockMediaDialogControllerDelegate& delegate() { return delegate_; }

 private:
  MockMediaDialogControllerDelegate delegate_;
  std::unique_ptr<MediaDialogController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MediaDialogControllerTest);
};

TEST_F(MediaDialogControllerTest, ShowControllableOnGainAndHideOnLoss) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  EXPECT_CALL(delegate(), ShowMediaSession(id.ToString(), _));

  SimulateFocusGained(id, true);
  SimulateNecessaryMetadata(id);

  // Ensure that the session was shown.
  testing::Mock::VerifyAndClearExpectations(&delegate());

  EXPECT_CALL(delegate(), HideMediaSession(id.ToString()));
  SimulateFocusLost(id);
}

TEST_F(MediaDialogControllerTest, DoesNotShowUncontrollableSession) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  EXPECT_CALL(delegate(), ShowMediaSession(_, _)).Times(0);

  SimulateFocusGained(id, false);
  SimulateNecessaryMetadata(id);
}
