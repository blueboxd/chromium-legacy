// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_impl.h"

#include <memory>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/media_controls/touchless/media_controls.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element_controls_list.h"
#include "third_party/blink/renderer/core/html/media/html_media_test_helper.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/time_ranges.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_list.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/test_media_controls_menu_host.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

const char kTextTracksOffString[] = "Off";

class LocalePlatformSupport : public TestingPlatformSupport {
 public:
  WebString QueryLocalizedString(WebLocalizedString::Name name) override {
    if (name == WebLocalizedString::kTextTracksOff)
      return kTextTracksOffString;
    return TestingPlatformSupport::QueryLocalizedString(name);
  }
};

class MockWebMediaPlayerForTouchlessImpl : public EmptyWebMediaPlayer {
 public:
  WebTimeRanges Seekable() const override { return seekable_; }
  bool HasVideo() const override { return true; }
  bool HasAudio() const override { return has_audio_; }
  WebTimeRanges Buffered() const override { return buffered_; }

  WebTimeRanges buffered_;
  WebTimeRanges seekable_;
  bool has_audio_ = false;
};

}  // namespace

class MockChromeClientForTouchlessImpl : public EmptyChromeClient {
 public:
  explicit MockChromeClientForTouchlessImpl()
      : orientation_(kWebScreenOrientationPortraitPrimary) {}

  WebScreenInfo GetScreenInfo() const override {
    WebScreenInfo screen_info;
    screen_info.orientation_type = orientation_;
    return screen_info;
  }

  void EnterFullscreen(LocalFrame& frame, const FullscreenOptions*) final {
    Fullscreen::DidEnterFullscreen(*frame.GetDocument());
  }

  void ExitFullscreen(LocalFrame& frame) final {
    Fullscreen::DidExitFullscreen(*frame.GetDocument());
  }

  void SetOrientation(WebScreenOrientationType orientation_type) {
    orientation_ = orientation_type;
  }

 private:
  WebScreenOrientationType orientation_;
};

class MediaControlsTouchlessImplTest : public PageTestBase {
 protected:
  void SetUp() override { InitializePage(); }

  void InitializePage() {
    Page::PageClients clients;

    FillWithEmptyClients(clients);
    chrome_client_ = MakeGarbageCollected<MockChromeClientForTouchlessImpl>();
    clients.chrome_client = chrome_client_;
    SetupPageWithClients(
        &clients, MakeGarbageCollected<test::MediaStubLocalFrameClient>(
                      std::make_unique<MockWebMediaPlayerForTouchlessImpl>()));

    GetDocument().write("<video controls>");
    HTMLMediaElement& video =
        ToHTMLVideoElement(*GetDocument().QuerySelector("video"));
    media_controls_ =
        static_cast<MediaControlsTouchlessImpl*>(video.GetMediaControls());

    test_media_controls_host_ = std::make_unique<TestMediaControlsMenuHost>();

    media_controls_->SetMediaControlsMenuHostForTesting(
        test_media_controls_host_->CreateMediaControlsMenuHostPtr());

    // Scripts are disabled by default which forces controls to be on.
    GetFrame().GetSettings()->SetScriptEnabled(true);
  }

  MediaControlsTouchlessImpl& MediaControls() { return *media_controls_; }
  HTMLMediaElement& MediaElement() { return MediaControls().MediaElement(); }

  MockWebMediaPlayerForTouchlessImpl* WebMediaPlayer() {
    return static_cast<MockWebMediaPlayerForTouchlessImpl*>(
        MediaElement().GetWebMediaPlayer());
  }

  TestMenuHostArgList& GetMenuHostArgList() {
    return test_media_controls_host_->GetMenuHostArgList();
  }

  void SetNetworkState(HTMLMediaElement::NetworkState state) {
    MediaElement().SetNetworkState(state);
  }

  void SetReadyState(HTMLMediaElement::ReadyState state) {
    MediaElement().SetReadyState(state);
  }

  void SimulateKeydownEvent(Element& element, int key_code) {
    KeyboardEventInit* keyboard_event_init = KeyboardEventInit::Create();
    keyboard_event_init->setKeyCode(key_code);

    Event* keyboard_event =
        MakeGarbageCollected<KeyboardEvent>("keydown", keyboard_event_init);
    element.DispatchEvent(*keyboard_event);
  }

  void LoadMediaWithDuration(double duration) {
    MediaElement().SetSrc("https://example.com/foo.mp4");
    test::RunPendingTasks();
    WebTimeRange time_range(0.0, duration);
    WebMediaPlayer()->seekable_.Assign(&time_range, 1);
    MediaElement().DurationChanged(duration, false /* requestSeek */);
  }

  void SetBufferedRange(double end) {
    WebTimeRange time_range(0.0, end);
    WebMediaPlayer()->buffered_.Assign(&time_range, 1);
  }

  bool IsControlsVisible(Element* element) {
    return !element->classList().contains("transparent") &&
           !element->classList().contains("transparent-immediate");
  }

  bool IsElementDisplayed(Element* element) {
    if (!element->InlineStyle())
      return true;

    return element->InlineStyle()->GetPropertyValue(CSSPropertyID::kDisplay) !=
           "none";
  }

  void SetHasAudio(bool has_audio) { WebMediaPlayer()->has_audio_ = has_audio; }

  Element* GetControlByShadowPseudoId(const char* shadow_pseudo_id) {
    for (Element& element : ElementTraversal::DescendantsOf(MediaControls())) {
      if (element.ShadowPseudoId() == shadow_pseudo_id)
        return &element;
    }
    return nullptr;
  }

  void SetScreenOrientation(WebScreenOrientationType orientation_type) {
    chrome_client_->SetOrientation(orientation_type);
  }

  void SetMenuResponse(mojom::blink::MenuItem menu_item, int track_index = -1) {
    test_media_controls_host_->SetMenuResponse(menu_item, track_index);
  }

  void SetMenuResponseAndShowMenu(mojom::blink::MenuItem menu_item,
                                  int track_index = -1) {
    SetMenuResponse(menu_item, track_index);
    MediaControls().ShowContextMenu();
    MediaControls().MenuHostFlushForTesting();
  }

  void CheckControlKeys(int seek_forward_key,
                        int seek_backward_key,
                        int volume_up_key,
                        int volume_down_key) {
    const int kNumberOfSecondsToJump = 10;
    const double kVolumeToChange = 0.05;
    const int initTime = 10;
    const double initVolume = 0.5;
    MediaElement().setCurrentTime(initTime);
    MediaElement().setVolume(initVolume);

    SimulateKeydownEvent(MediaElement(), seek_forward_key);
    ASSERT_EQ(MediaElement().currentTime(), initTime + kNumberOfSecondsToJump);

    SimulateKeydownEvent(MediaElement(), seek_backward_key);
    ASSERT_EQ(MediaElement().currentTime(), initTime);

    SimulateKeydownEvent(MediaElement(), volume_up_key);
    ASSERT_EQ(MediaElement().volume(), initVolume + kVolumeToChange);

    SimulateKeydownEvent(MediaElement(), volume_down_key);
    ASSERT_EQ(MediaElement().volume(), initVolume);
  }

 private:
  Persistent<MediaControlsTouchlessImpl> media_controls_;
  Persistent<MockChromeClientForTouchlessImpl> chrome_client_;
  std::unique_ptr<TestMediaControlsMenuHost> test_media_controls_host_;
};

class MediaControlsTouchlessImplTestWithMockScheduler
    : public MediaControlsTouchlessImplTest {
 public:
  MediaControlsTouchlessImplTestWithMockScheduler() { EnablePlatform(); }

 protected:
  void SetUp() override {
    platform()->AdvanceClockSeconds(1);
    MediaControlsTouchlessImplTest::SetUp();
  }
};

TEST_F(MediaControlsTouchlessImplTest, PlayPause) {
  MediaElement().SetFocused(true, WebFocusType::kWebFocusTypeNone);
  MediaElement().Play();
  ASSERT_FALSE(MediaElement().paused());

  // Press center key and video should be paused.
  SimulateKeydownEvent(MediaElement(), VKEY_RETURN);
  ASSERT_TRUE(MediaElement().paused());

  // Press center key and video should be played.
  SimulateKeydownEvent(MediaElement(), VKEY_RETURN);
  ASSERT_FALSE(MediaElement().paused());
}

TEST_F(MediaControlsTouchlessImplTest, HandlesOrientationForArrowInput) {
  MediaElement().SetFocused(true, WebFocusType::kWebFocusTypeNone);

  SetScreenOrientation(kWebScreenOrientationPortraitPrimary);
  CheckControlKeys(VKEY_RIGHT, VKEY_LEFT, VKEY_UP, VKEY_DOWN);

  SetScreenOrientation(kWebScreenOrientationLandscapePrimary);
  CheckControlKeys(VKEY_DOWN, VKEY_UP, VKEY_RIGHT, VKEY_LEFT);

  SetScreenOrientation(kWebScreenOrientationPortraitSecondary);
  CheckControlKeys(VKEY_LEFT, VKEY_RIGHT, VKEY_DOWN, VKEY_UP);

  SetScreenOrientation(kWebScreenOrientationLandscapeSecondary);
  CheckControlKeys(VKEY_UP, VKEY_DOWN, VKEY_LEFT, VKEY_RIGHT);
}

TEST_F(MediaControlsTouchlessImplTest, ArrowInputEdgeCaseHandling) {
  const double duration = 100;

  LoadMediaWithDuration(duration);
  MediaElement().SetFocused(true, WebFocusType::kWebFocusTypeNone);

  // Seek backward at low current time
  MediaElement().setCurrentTime(1);
  SimulateKeydownEvent(MediaElement(), VKEY_LEFT);
  ASSERT_EQ(MediaElement().currentTime(), 0);

  // Seek forward at high current time
  MediaElement().setCurrentTime(duration - 1);
  SimulateKeydownEvent(MediaElement(), VKEY_RIGHT);
  ASSERT_EQ(MediaElement().currentTime(), duration);

  // Volume down at low volume
  MediaElement().setVolume(0.01);
  SimulateKeydownEvent(MediaElement(), VKEY_DOWN);
  ASSERT_EQ(MediaElement().volume(), 0);

  // Volume up at high volume
  MediaElement().setVolume(0.99);
  SimulateKeydownEvent(MediaElement(), VKEY_UP);
  ASSERT_EQ(MediaElement().volume(), 1);
}

TEST_F(MediaControlsTouchlessImplTest, PlayPauseIcon) {
  MediaElement().SetFocused(true, WebFocusType::kWebFocusTypeNone);

  Element* play_button = GetControlByShadowPseudoId(
      "-internal-media-controls-touchless-play-button");
  ASSERT_NE(nullptr, play_button);

  MediaElement().pause();

  MediaElement().Play();
  test::RunPendingTasks();
  ASSERT_TRUE(play_button->classList().contains("playing"));
  ASSERT_FALSE(play_button->classList().contains("paused"));

  MediaElement().pause();
  test::RunPendingTasks();
  ASSERT_TRUE(play_button->classList().contains("paused"));
  ASSERT_FALSE(play_button->classList().contains("playing"));
}

TEST_F(MediaControlsTouchlessImplTest, ProgressBar) {
  const double duration = 100.0;
  const double buffered = 60.0;
  const double current_time = 15.0;

  LoadMediaWithDuration(duration);
  SetBufferedRange(buffered);
  MediaElement().setCurrentTime(current_time);
  test::RunPendingTasks();

  MediaElement().DispatchEvent(*Event::Create(event_type_names::kTimeupdate));

  Element* timeline =
      GetControlByShadowPseudoId("-internal-media-controls-touchless-timeline");
  Element* progress_bar = GetControlByShadowPseudoId(
      "-internal-media-controls-touchless-timeline-progress");
  Element* loaded_bar = GetControlByShadowPseudoId(
      "-internal-media-controls-touchless-timeline-loaded");

  ASSERT_NE(nullptr, timeline);
  ASSERT_NE(nullptr, progress_bar);
  ASSERT_NE(nullptr, loaded_bar);

  double timeline_width = timeline->getBoundingClientRect()->width();
  double progress_bar_width = progress_bar->getBoundingClientRect()->width();
  double loaded_bar_width = loaded_bar->getBoundingClientRect()->width();
  ASSERT_GT(timeline_width, 0);

  EXPECT_DOUBLE_EQ(buffered / duration, loaded_bar_width / timeline_width);
  EXPECT_DOUBLE_EQ(current_time / buffered,
                   progress_bar_width / loaded_bar_width);

  // Seek event should trigger a UI update as well.
  SetBufferedRange(0);
  MediaElement().setCurrentTime(0);
  MediaElement().DispatchEvent(*Event::Create(event_type_names::kSeeking));

  EXPECT_DOUBLE_EQ(progress_bar->getBoundingClientRect()->width(), 0);
  EXPECT_DOUBLE_EQ(loaded_bar->getBoundingClientRect()->width(), 0);
}

TEST_F(MediaControlsTouchlessImplTest, TimeDisplay) {
  const double duration = 4000;
  const double current_time = 3599;
  const char expect_display[] = "59:59 / 1:06:40";

  Element* time_display = GetControlByShadowPseudoId(
      "-internal-media-controls-touchless-time-display");

  EXPECT_EQ(time_display->InnerHTMLAsString(), "0:00 / 0:00");

  LoadMediaWithDuration(duration);
  MediaElement().setCurrentTime(current_time);
  test::RunPendingTasks();
  MediaElement().DispatchEvent(*Event::Create(event_type_names::kTimeupdate));

  EXPECT_EQ(time_display->InnerHTMLAsString(), expect_display);
}

TEST_F(MediaControlsTouchlessImplTest, VolumeDisplayTest) {
  Element* volume_bar_background = GetControlByShadowPseudoId(
      "-internal-media-controls-touchless-volume-bar-background");
  Element* volume_bar = GetControlByShadowPseudoId(
      "-internal-media-controls-touchless-volume-bar");
  ASSERT_NE(nullptr, volume_bar_background);
  ASSERT_NE(nullptr, volume_bar);

  const double volume = 0.65;        // Initial volume.
  const double volume_delta = 0.05;  // Volume change for each press.
  const double error = 0.01;         // Allow precision error.
  MediaElement().setVolume(volume);
  SimulateKeydownEvent(MediaElement(), VK_UP);

  double volume_bar_background_height =
      volume_bar_background->getBoundingClientRect()->height();
  double volume_bar_height = volume_bar->getBoundingClientRect()->height();

  EXPECT_NEAR(volume + volume_delta,
              volume_bar_height / volume_bar_background_height, error);
}

TEST_F(MediaControlsTouchlessImplTest, ContextMenuMojomTest) {
  ScopedTestingPlatformSupport<LocalePlatformSupport> support;

  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(GetDocument().GetFrame(),
                                       UserGestureToken::kNewGesture);
  test::RunPendingTasks();

  WebKeyboardEvent web_keyboard_event(
      WebInputEvent::kKeyUp, WebInputEvent::Modifiers::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  // TODO: Cleanup magic numbers once https://crbug.com/949766 lands.
  web_keyboard_event.dom_key = 0x00200310;
  Event* keyboard_event = KeyboardEvent::Create(web_keyboard_event, nullptr);

  // Test fullscreen function.
  SetMenuResponse(mojom::blink::MenuItem::FULLSCREEN);
  MediaElement().DispatchEvent(*keyboard_event);
  MediaControls().MenuHostFlushForTesting();
  test::RunPendingTasks();

  TestMenuHostArgList& arg_list = GetMenuHostArgList();
  EXPECT_EQ((int)arg_list.menu_items.size(), 2);
  EXPECT_EQ(arg_list.menu_items[0], mojom::blink::MenuItem::FULLSCREEN);
  EXPECT_EQ(arg_list.menu_items[1], mojom::blink::MenuItem::DOWNLOAD);
  EXPECT_FALSE(arg_list.video_state->is_fullscreen);
  EXPECT_TRUE(MediaElement().IsFullscreen());

  SetMenuResponseAndShowMenu(mojom::blink::MenuItem::FULLSCREEN);
  test::RunPendingTasks();

  EXPECT_TRUE(arg_list.video_state->is_fullscreen);
  EXPECT_FALSE(MediaElement().IsFullscreen());

  // Disable download and fullscreen, and show mute option.
  MediaElement().ControlsListInternal()->Add("nofullscreen");
  MediaElement().GetDocument().GetSettings()->SetHideDownloadUI(true);
  SetHasAudio(true);

  SetMenuResponseAndShowMenu(mojom::blink::MenuItem::MUTE);

  EXPECT_EQ((int)arg_list.menu_items.size(), 1);
  EXPECT_EQ(arg_list.menu_items[0], mojom::blink::MenuItem::MUTE);
  EXPECT_FALSE(arg_list.video_state->is_muted);
  EXPECT_TRUE(MediaElement().muted());

  SetMenuResponseAndShowMenu(mojom::blink::MenuItem::MUTE);

  EXPECT_TRUE(arg_list.video_state->is_muted);
  EXPECT_FALSE(MediaElement().muted());

  // Disable mute option and show text track option.
  SetHasAudio(false);
  TextTrack* track = MediaElement().addTextTrack("subtitles", "english", "en",
                                                 ASSERT_NO_EXCEPTION);
  SetMenuResponseAndShowMenu(mojom::blink::MenuItem::CAPTIONS, 0);

  EXPECT_EQ((int)arg_list.menu_items.size(), 1);
  EXPECT_EQ(arg_list.menu_items[0], mojom::blink::MenuItem::CAPTIONS);
  EXPECT_EQ(arg_list.text_tracks[1]->label, "english");
  EXPECT_EQ(track->mode(), TextTrack::ShowingKeyword());

  SetMenuResponseAndShowMenu(mojom::blink::MenuItem::CAPTIONS, -1);
  EXPECT_NE(track->mode(), TextTrack::ShowingKeyword());
}

TEST_F(MediaControlsTouchlessImplTest, NoSourceTest) {
  EXPECT_TRUE(MediaControls().classList().contains("state-no-source"));

  LoadMediaWithDuration(10);
  EXPECT_FALSE(MediaControls().classList().contains("state-no-source"));
}

TEST_F(MediaControlsTouchlessImplTest, DefaultPosterTest) {
  LoadMediaWithDuration(10);

  SetNetworkState(HTMLMediaElement::NetworkState::kNetworkLoading);
  test::RunPendingTasks();
  EXPECT_TRUE(MediaControls().classList().contains("use-default-poster"));

  SetNetworkState(HTMLMediaElement::NetworkState::kNetworkIdle);
  SetReadyState(HTMLMediaElement::ReadyState::kHaveMetadata);
  test::RunPendingTasks();
  EXPECT_FALSE(MediaControls().classList().contains("use-default-poster"));
}

TEST_F(MediaControlsTouchlessImplTest, DoesNotHandleKeysWhenDisabled) {
  // Disable the controls.
  MediaElement().SetBooleanAttribute(html_names::kControlsAttr, false);

  // Focus the video.
  MediaElement().SetFocused(true, WebFocusType::kWebFocusTypeNone);
  ASSERT_TRUE(MediaElement().paused());

  // Enter should not play the video.
  SimulateKeydownEvent(MediaElement(), VKEY_RETURN);
  ASSERT_TRUE(MediaElement().paused());

  // The arrow keys should also not be handled.
  const int initTime = 10;
  const double initVolume = 0.5;
  MediaElement().setCurrentTime(initTime);
  MediaElement().setVolume(initVolume);

  SimulateKeydownEvent(MediaElement(), VKEY_RIGHT);
  ASSERT_EQ(MediaElement().currentTime(), initTime);

  SimulateKeydownEvent(MediaElement(), VKEY_LEFT);
  ASSERT_EQ(MediaElement().currentTime(), initTime);

  SimulateKeydownEvent(MediaElement(), VKEY_UP);
  ASSERT_EQ(MediaElement().volume(), initVolume);

  SimulateKeydownEvent(MediaElement(), VKEY_DOWN);
  ASSERT_EQ(MediaElement().volume(), initVolume);
}

TEST_F(MediaControlsTouchlessImplTestWithMockScheduler,
       MidOverlayHideTimerTest) {
  Element* overlay =
      GetControlByShadowPseudoId("-internal-media-controls-touchless-overlay");
  ASSERT_NE(nullptr, overlay);

  // Overlay should starts hidden.
  EXPECT_FALSE(IsControlsVisible(overlay));

  // Overlay should show when focus in.
  MediaElement().SetFocused(true, WebFocusType::kWebFocusTypeNone);
  MediaElement().DispatchEvent(*Event::Create(event_type_names::kFocusin));
  EXPECT_TRUE(IsControlsVisible(overlay));

  // Overlay should hide after 3 seconds.
  platform()->RunForPeriodSeconds(2.99);
  EXPECT_TRUE(IsControlsVisible(overlay));
  platform()->RunForPeriodSeconds(0.01);
  EXPECT_FALSE(IsControlsVisible(overlay));

  // Overlay should show upon pressing return key.
  SimulateKeydownEvent(MediaElement(), VKEY_RETURN);
  EXPECT_TRUE(IsControlsVisible(overlay));

  // Overlay should not disappear after 2 seconds.
  platform()->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsControlsVisible(overlay));

  // Overlay should hide 3 seconds after last key press.
  platform()->RunForPeriodSeconds(1);
  EXPECT_FALSE(IsControlsVisible(overlay));
}

TEST_F(MediaControlsTouchlessImplTestWithMockScheduler,
       BottomContainerHideTimerTest) {
  Element* bottom_container = GetControlByShadowPseudoId(
      "-internal-media-controls-touchless-bottom-container");
  ASSERT_NE(nullptr, bottom_container);

  // Bottom container starts opaque since video is paused.
  EXPECT_TRUE(IsControlsVisible(bottom_container));
  EXPECT_TRUE(IsElementDisplayed(bottom_container));

  MediaElement().Play();
  platform()->RunForPeriodSeconds(3);
  EXPECT_FALSE(IsControlsVisible(bottom_container));

  MediaElement().pause();

  // Pause after play should stop hide timer.
  MediaElement().Play();
  MediaElement().pause();
  platform()->RunForPeriodSeconds(5);
  EXPECT_TRUE(IsControlsVisible(bottom_container));

  MediaElement().Play();
  platform()->RunForPeriodSeconds(5);
  EXPECT_FALSE(IsControlsVisible(bottom_container));

  // Bottom container should show after focus in.
  MediaElement().SetFocused(true, WebFocusType::kWebFocusTypeNone);
  MediaElement().DispatchEvent(*Event::Create(event_type_names::kFocusin));
  EXPECT_TRUE(IsControlsVisible(bottom_container));

  // Hide after 3 seconds
  platform()->RunForPeriodSeconds(3);
  EXPECT_FALSE(IsControlsVisible(bottom_container));

  // Display should be none after hide transition ends.
  bottom_container->DispatchEvent(
      *Event::Create(event_type_names::kTransitionend));
  EXPECT_FALSE(IsElementDisplayed(bottom_container));

  // Bottom container should show after pressing right/left arrow.
  SimulateKeydownEvent(MediaElement(), VK_RIGHT);
  EXPECT_TRUE(IsControlsVisible(bottom_container));

  platform()->RunForPeriodSeconds(3);
  EXPECT_FALSE(IsControlsVisible(bottom_container));

  SimulateKeydownEvent(MediaElement(), VK_LEFT);
  EXPECT_TRUE(IsControlsVisible(bottom_container));

  platform()->RunForPeriodSeconds(3);
  EXPECT_FALSE(IsControlsVisible(bottom_container));
}

TEST_F(MediaControlsTouchlessImplTestWithMockScheduler,
       VolumeDisplayTimerTest) {
  Element* volume_container = GetControlByShadowPseudoId(
      "-internal-media-controls-touchless-volume-container");
  Element* overlay =
      GetControlByShadowPseudoId("-internal-media-controls-touchless-overlay");
  ASSERT_NE(nullptr, volume_container);
  ASSERT_NE(nullptr, overlay);

  MediaElement().SetFocused(true, WebFocusType::kWebFocusTypeNone);
  MediaElement().DispatchEvent(*Event::Create(event_type_names::kFocusin));
  EXPECT_TRUE(IsControlsVisible(overlay));

  // Press up button should bring up volume display and hide overlay
  // immediately.
  SimulateKeydownEvent(MediaElement(), VK_UP);
  EXPECT_TRUE(IsControlsVisible(volume_container));
  EXPECT_FALSE(IsControlsVisible(overlay));

  platform()->RunForPeriodSeconds(3);
  EXPECT_FALSE(IsControlsVisible(volume_container));

  SimulateKeydownEvent(MediaElement(), VK_UP);
  EXPECT_TRUE(IsControlsVisible(volume_container));

  // Press mid key should bring up mid overlay and hide volume display
  // immediately.
  SimulateKeydownEvent(MediaElement(), VK_RETURN);
  EXPECT_FALSE(IsControlsVisible(volume_container));
  EXPECT_TRUE(IsControlsVisible(overlay));
}

}  // namespace blink
