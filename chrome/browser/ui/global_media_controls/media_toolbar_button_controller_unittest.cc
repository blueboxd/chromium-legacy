// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/global_media_controls/media_dialog_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
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

class MockMediaToolbarButtonControllerDelegate
    : public MediaToolbarButtonControllerDelegate {
 public:
  MockMediaToolbarButtonControllerDelegate() = default;
  ~MockMediaToolbarButtonControllerDelegate() override = default;

  // MediaToolbarButtonControllerDelegate implementation.
  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD0(Enable, void());
  MOCK_METHOD0(Disable, void());
};

class MockMediaDialogDelegate : public MediaDialogDelegate {
 public:
  MockMediaDialogDelegate() = default;
  ~MockMediaDialogDelegate() override { Close(); }

  void Open(MediaToolbarButtonController* controller) {
    ASSERT_NE(nullptr, controller);
    controller_ = controller;
    controller_->SetDialogDelegate(this);
  }

  void Close() {
    if (!controller_)
      return;

    controller_->SetDialogDelegate(nullptr);
    controller_ = nullptr;
  }

  // MediaDialogDelegate implementation.
  MOCK_METHOD2(
      ShowMediaSession,
      void(const std::string& id,
           base::WeakPtr<media_message_center::MediaNotificationItem> item));
  MOCK_METHOD1(HideMediaSession, void(const std::string& id));

 private:
  MediaToolbarButtonController* controller_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaDialogDelegate);
};

}  // anonymous namespace

class MediaToolbarButtonControllerTest : public testing::Test {
 public:
  MediaToolbarButtonControllerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                          base::test::TaskEnvironment::MainThreadType::UI) {}
  ~MediaToolbarButtonControllerTest() override = default;

  void SetUp() override {
    controller_ = std::make_unique<MediaToolbarButtonController>(
        base::UnguessableToken::Create(), nullptr, &delegate_);
  }

 protected:
  void AdvanceClockMilliseconds(int milliseconds) {
    task_environment_.FastForwardBy(
        base::TimeDelta::FromMilliseconds(milliseconds));
  }

  base::UnguessableToken SimulatePlayingControllableMedia() {
    base::UnguessableToken id = base::UnguessableToken::Create();
    SimulateFocusGained(id, true);
    SimulateNecessaryMetadata(id);
    return id;
  }

  AudioFocusRequestStatePtr CreateFocusRequest(const base::UnguessableToken& id,
                                               bool controllable) {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->is_controllable = controllable;

    AudioFocusRequestStatePtr focus(AudioFocusRequestState::New());
    focus->request_id = id;
    focus->session_info = std::move(session_info);
    return focus;
  }

  void SimulateFocusGained(const base::UnguessableToken& id,
                           bool controllable) {
    controller_->OnFocusGained(CreateFocusRequest(id, controllable));
  }

  void SimulateFocusLost(const base::UnguessableToken& id) {
    AudioFocusRequestStatePtr focus(AudioFocusRequestState::New());
    focus->request_id = id;
    controller_->OnFocusLost(std::move(focus));
  }

  void SimulateNecessaryMetadata(const base::UnguessableToken& id) {
    // In order for the MediaNotificationItem to tell the
    // MediaToolbarButtonController to show a media session, that session needs
    // a title and artist. Typically this would happen through the media session
    // service, but since the service doesn't run for this test, we'll manually
    // grab the MediaNotificationItem from the MediaToolbarButtonController and
    // set the metadata.
    auto item_itr = controller_->sessions_.find(id.ToString());
    ASSERT_NE(controller_->sessions_.end(), item_itr);

    media_session::MediaMetadata metadata;
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    item_itr->second.item()->MediaSessionMetadataChanged(std::move(metadata));
  }

  void SimulateReceivedAudioFocusRequests(
      std::vector<AudioFocusRequestStatePtr> requests) {
    controller_->OnReceivedAudioFocusRequests(std::move(requests));
  }

  bool IsSessionFrozen(const base::UnguessableToken& id) const {
    auto item_itr = controller_->sessions_.find(id.ToString());
    EXPECT_NE(controller_->sessions_.end(), item_itr);
    return item_itr->second.item()->frozen();
  }

  void SimulateDialogOpened(MockMediaDialogDelegate* delegate) {
    delegate->Open(controller_.get());
  }

  void SimulateTabClosed(const base::UnguessableToken& id) {
    // When a tab is closing, audio focus will be lost before the WebContents is
    // destroyed, so to simulate closer to reality we will also simulate audio
    // focus lost here.
    SimulateFocusLost(id);

    // Now, close the tab.
    auto item_itr = controller_->sessions_.find(id.ToString());
    EXPECT_NE(controller_->sessions_.end(), item_itr);
    item_itr->second.WebContentsDestroyed();
  }

  void SimulateDismissButtonClicked(const base::UnguessableToken& id) {
    controller_->OnDismissButtonClicked(id.ToString());
  }

  void ExpectHistogramCountRecorded(int count, int size) {
    histogram_tester_.ExpectBucketCount(
        media_message_center::kCountHistogramName, count, size);
  }

  MockMediaToolbarButtonControllerDelegate& delegate() { return delegate_; }

 private:
  base::test::TaskEnvironment task_environment_;
  MockMediaToolbarButtonControllerDelegate delegate_;
  std::unique_ptr<MediaToolbarButtonController> controller_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(MediaToolbarButtonControllerTest);
};

TEST_F(MediaToolbarButtonControllerTest, ShowControllableOnGainAndHideOnLoss) {
  // Simulate a new active, controllable media session.
  EXPECT_CALL(delegate(), Show());
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_FALSE(IsSessionFrozen(id));

  // Ensure that the toolbar button was shown.
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Simulate opening a MediaDialogView.
  MockMediaDialogDelegate dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaSession(id.ToString(), _));
  SimulateDialogOpened(&dialog_delegate);

  // Ensure that the session was shown.
  ExpectHistogramCountRecorded(1, 1);
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);

  // Simulate the active session ending.
  EXPECT_CALL(dialog_delegate, HideMediaSession(id.ToString())).Times(0);
  SimulateFocusLost(id);

  // Ensure that the session was frozen and not hidden.
  EXPECT_TRUE(IsSessionFrozen(id));
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);

  // Once the freeze timer fires, we should hide the media session.
  EXPECT_CALL(dialog_delegate, HideMediaSession(id.ToString()));
  AdvanceClockMilliseconds(2500);
}

TEST_F(MediaToolbarButtonControllerTest, DoesNotShowUncontrollableSession) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  EXPECT_CALL(delegate(), Show()).Times(0);

  SimulateFocusGained(id, false);
  SimulateNecessaryMetadata(id);
}

TEST_F(MediaToolbarButtonControllerTest, ShowsAllInitialControllableSessions) {
  base::UnguessableToken controllable1_id = base::UnguessableToken::Create();
  base::UnguessableToken uncontrollable_id = base::UnguessableToken::Create();
  base::UnguessableToken controllable2_id = base::UnguessableToken::Create();

  std::vector<AudioFocusRequestStatePtr> requests;
  requests.push_back(CreateFocusRequest(controllable1_id, true));
  requests.push_back(CreateFocusRequest(uncontrollable_id, false));
  requests.push_back(CreateFocusRequest(controllable2_id, true));

  // Having active, controllable sessions should show the toolbar button.
  EXPECT_CALL(delegate(), Show());

  SimulateReceivedAudioFocusRequests(std::move(requests));

  SimulateNecessaryMetadata(controllable1_id);
  SimulateNecessaryMetadata(uncontrollable_id);
  SimulateNecessaryMetadata(controllable2_id);

  testing::Mock::VerifyAndClearExpectations(&delegate());

  // If we open a dialog, it should be told to show the controllable sessions,
  // but not the uncontrollable one.
  MockMediaDialogDelegate dialog_delegate;

  EXPECT_CALL(dialog_delegate,
              ShowMediaSession(controllable1_id.ToString(), _));
  EXPECT_CALL(dialog_delegate,
              ShowMediaSession(uncontrollable_id.ToString(), _))
      .Times(0);
  EXPECT_CALL(dialog_delegate,
              ShowMediaSession(controllable2_id.ToString(), _));
  SimulateDialogOpened(&dialog_delegate);

  // Ensure that we properly recorded the number of active sessions shown.
  ExpectHistogramCountRecorded(2, 1);
}

TEST_F(MediaToolbarButtonControllerTest, HidesAfterTimeoutAndShowsAgainOnPlay) {
  // First, show the button.
  EXPECT_CALL(delegate(), Show());
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Then, stop playing media so the button is disabled, but hasn't been hidden
  // yet.
  EXPECT_CALL(delegate(), Disable());
  EXPECT_CALL(delegate(), Hide()).Times(0);
  SimulateFocusLost(id);
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // If the time hasn't elapsed yet, the button should still not be hidden.
  EXPECT_CALL(delegate(), Hide()).Times(0);
  AdvanceClockMilliseconds(2400);
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Once the time is elapsed, the button should be hidden.
  EXPECT_CALL(delegate(), Hide());
  AdvanceClockMilliseconds(200);
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // If media starts playing again, we should show and enable the button.
  EXPECT_CALL(delegate(), Show());
  EXPECT_CALL(delegate(), Enable());
  SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());
}

TEST_F(MediaToolbarButtonControllerTest, DoesNotDisableButtonIfDialogIsOpen) {
  // First, show the button.
  EXPECT_CALL(delegate(), Show());
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Then, open a dialog.
  MockMediaDialogDelegate dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaSession(id.ToString(), _));
  SimulateDialogOpened(&dialog_delegate);

  // Then, have the session lose focus. This should not disable the button when
  // a dialog is present (since the button can still be used to close the
  // dialog).
  EXPECT_CALL(delegate(), Disable()).Times(0);
  SimulateFocusLost(id);
  testing::Mock::VerifyAndClearExpectations(&delegate());
}

TEST_F(MediaToolbarButtonControllerTest,
       DoesNotHideIfMediaStartsPlayingWithinTimeout) {
  // First, show the button.
  EXPECT_CALL(delegate(), Show());
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Then, stop playing media so the button is disabled, but hasn't been hidden
  // yet.
  EXPECT_CALL(delegate(), Disable());
  EXPECT_CALL(delegate(), Hide()).Times(0);
  SimulateFocusLost(id);
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // If the time hasn't elapsed yet, we should still not be hidden.
  EXPECT_CALL(delegate(), Hide()).Times(0);
  AdvanceClockMilliseconds(2400);
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // If media starts playing again, we should show and enable the button.
  EXPECT_CALL(delegate(), Show());
  EXPECT_CALL(delegate(), Enable());
  SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());
}

TEST_F(MediaToolbarButtonControllerTest, NewMediaSessionWhileDialogOpen) {
  // First, show the button.
  EXPECT_CALL(delegate(), Show());
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Then, open a dialog.
  MockMediaDialogDelegate dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaSession(id.ToString(), _));
  SimulateDialogOpened(&dialog_delegate);
  ExpectHistogramCountRecorded(1, 1);
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);

  // Then, have a new media session start while the dialog is opened. This
  // should update the dialog.
  base::UnguessableToken new_id = base::UnguessableToken::Create();
  EXPECT_CALL(dialog_delegate, ShowMediaSession(new_id.ToString(), _));
  SimulateFocusGained(new_id, true);
  SimulateNecessaryMetadata(new_id);
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);

  // If we close this dialog and open a new one, the new one should receive both
  // media sessions immediately.
  dialog_delegate.Close();
  MockMediaDialogDelegate new_dialog;
  EXPECT_CALL(new_dialog, ShowMediaSession(id.ToString(), _));
  EXPECT_CALL(new_dialog, ShowMediaSession(new_id.ToString(), _));
  SimulateDialogOpened(&new_dialog);
  ExpectHistogramCountRecorded(1, 1);
  ExpectHistogramCountRecorded(2, 1);
}

TEST_F(MediaToolbarButtonControllerTest,
       SessionIsRemovedImmediatelyWhenATabCloses) {
  // First, show the button.
  EXPECT_CALL(delegate(), Show());
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Then, close the tab. The button should immediately hide.
  EXPECT_CALL(delegate(), Hide());
  SimulateTabClosed(id);
  testing::Mock::VerifyAndClearExpectations(&delegate());
}

TEST_F(MediaToolbarButtonControllerTest, DismissesMediaSession) {
  // First, show the button.
  EXPECT_CALL(delegate(), Show());
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Then, open a dialog.
  MockMediaDialogDelegate dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaSession(id.ToString(), _));
  SimulateDialogOpened(&dialog_delegate);

  // Then, click the dismiss button. This should stop and hide the session.
  EXPECT_CALL(dialog_delegate, HideMediaSession(id.ToString()));
  SimulateDismissButtonClicked(id);
  testing::Mock::VerifyAndClearExpectations(&delegate());
}
