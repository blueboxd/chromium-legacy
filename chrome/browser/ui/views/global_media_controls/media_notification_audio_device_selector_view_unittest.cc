// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_audio_device_selector_view.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/util/ranges/algorithm.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_audio_device_selector_view_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "media/audio/audio_device_description.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/color_palette.h"

class MediaNotificationContainerObserver;

namespace {

class MockMediaNotificationDeviceProvider
    : public MediaNotificationDeviceProvider {
 public:
  MockMediaNotificationDeviceProvider() = default;
  ~MockMediaNotificationDeviceProvider() override = default;

  void AddDevice(const std::string& device_name, const std::string& device_id) {
    device_descriptions_.emplace_back(device_name, device_id, "");
  }

  void ResetDevices() { device_descriptions_.clear(); }

  void RunUICallback() { output_devices_callback_.Run(device_descriptions_); }

  std::unique_ptr<MediaNotificationDeviceProvider::
                      GetOutputDevicesCallbackList::Subscription>
  RegisterOutputDeviceDescriptionsCallback(
      GetOutputDevicesCallback cb) override {
    output_devices_callback_ = std::move(cb);
    RunUICallback();
    return std::unique_ptr<MockMediaNotificationDeviceProvider::
                               GetOutputDevicesCallbackList::Subscription>(
        nullptr);
  }

  MOCK_METHOD(void,
              GetOutputDeviceDescriptions,
              (media::AudioSystem::OnDeviceDescriptionsCallback),
              (override));

 private:
  media::AudioDeviceDescriptions device_descriptions_;

  GetOutputDevicesCallback output_devices_callback_;
};

class MockMediaNotificationAudioDeviceSelectorViewDelegate
    : public MediaNotificationAudioDeviceSelectorViewDelegate {
 public:
  MOCK_METHOD(void,
              OnAudioSinkChosen,
              (const std::string& sink_id),
              (override));
  MOCK_METHOD(void, OnAudioDeviceSelectorViewSizeChanged, (), (override));
};

}  // anonymous namespace

class MediaNotificationAudioDeviceSelectorViewTest
    : public ChromeViewsTestBase {
 public:
  MediaNotificationAudioDeviceSelectorViewTest() = default;
  ~MediaNotificationAudioDeviceSelectorViewTest() override = default;

  // ChromeViewsTestBase
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    provider_ = std::make_unique<MockMediaNotificationDeviceProvider>();
    media_router::MediaRouterFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&media_router::MockMediaRouter::Create));
    service_ = std::make_unique<MediaNotificationService>(&profile_);
  }

  void TearDown() override {
    view_.reset();
    service_.reset();
    provider_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void SimulateButtonClick(views::View* view) {
    view_->ButtonPressed(
        static_cast<views::Button*>(view),
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0));
  }

  static std::string EntryLabelText(views::View* entry_view) {
    return MediaNotificationAudioDeviceSelectorView::
        get_entry_label_for_testing(entry_view);
  }

  static bool IsHighlighted(views::View* entry_view) {
    return MediaNotificationAudioDeviceSelectorView::
        get_entry_is_highlighted_for_testing(entry_view);
  }

  std::string GetButtonText(views::View* view) {
    return base::UTF16ToUTF8(static_cast<views::LabelButton*>(view)->GetText());
  }

  TestingProfile profile_;
  std::unique_ptr<MockMediaNotificationDeviceProvider> provider_;
  std::unique_ptr<MediaNotificationService> service_;
  std::unique_ptr<MediaNotificationAudioDeviceSelectorView> view_;
};

TEST_F(MediaNotificationAudioDeviceSelectorViewTest, DeviceButtonsCreated) {
  // Buttons should be created for every device reported by the provider
  provider_->AddDevice("Speaker", "1");
  provider_->AddDevice("Headphones", "2");
  provider_->AddDevice("Earbuds", "3");
  service_->set_device_provider_for_testing(std::move(provider_));

  MockMediaNotificationAudioDeviceSelectorViewDelegate delegate;
  view_ = std::make_unique<MediaNotificationAudioDeviceSelectorView>(
      &delegate, service_.get(), "1", gfx::kPlaceholderColor,
      gfx::kPlaceholderColor);

  ASSERT_TRUE(view_->audio_device_entries_container_ != nullptr);

  auto container_children = view_->audio_device_entries_container_->children();
  ASSERT_EQ(container_children.size(), 3u);

  EXPECT_EQ(EntryLabelText(container_children.at(0)), "Speaker");
  EXPECT_EQ(EntryLabelText(container_children.at(1)), "Headphones");
  EXPECT_EQ(EntryLabelText(container_children.at(2)), "Earbuds");
}

TEST_F(MediaNotificationAudioDeviceSelectorViewTest,
       ExpandButtonOpensEntryContainer) {
  provider_->AddDevice("Speaker", "1");
  service_->set_device_provider_for_testing(std::move(provider_));
  MockMediaNotificationAudioDeviceSelectorViewDelegate delegate;
  view_ = std::make_unique<MediaNotificationAudioDeviceSelectorView>(
      &delegate, service_.get(), "1", gfx::kPlaceholderColor,
      gfx::kPlaceholderColor);

  ASSERT_TRUE(view_->expand_button_);
  EXPECT_FALSE(view_->audio_device_entries_container_->GetVisible());
  SimulateButtonClick(view_->expand_button_);
  EXPECT_TRUE(view_->audio_device_entries_container_->GetVisible());
}

TEST_F(MediaNotificationAudioDeviceSelectorViewTest,
       DeviceButtonClickNotifiesContainer) {
  // When buttons are clicked the media notification delegate should be
  // informed.
  provider_->AddDevice("Speaker", "1");
  provider_->AddDevice("Headphones", "2");
  provider_->AddDevice("Earbuds", "3");
  service_->set_device_provider_for_testing(std::move(provider_));

  MockMediaNotificationAudioDeviceSelectorViewDelegate delegate;
  EXPECT_CALL(delegate, OnAudioSinkChosen("1")).Times(1);
  EXPECT_CALL(delegate, OnAudioSinkChosen("2")).Times(1);
  EXPECT_CALL(delegate, OnAudioSinkChosen("3")).Times(1);

  view_ = std::make_unique<MediaNotificationAudioDeviceSelectorView>(
      &delegate, service_.get(), "1", gfx::kPlaceholderColor,
      gfx::kPlaceholderColor);

  for (views::View* child :
       view_->audio_device_entries_container_->children()) {
    SimulateButtonClick(child);
  }
}

TEST_F(MediaNotificationAudioDeviceSelectorViewTest, CurrentDeviceHighlighted) {
  // The 'current' audio device should be highlighted in the UI and appear
  // before other devices.
  provider_->AddDevice("Speaker", "1");
  provider_->AddDevice("Headphones", "2");
  provider_->AddDevice("Earbuds", "3");
  service_->set_device_provider_for_testing(std::move(provider_));

  MockMediaNotificationAudioDeviceSelectorViewDelegate delegate;
  view_ = std::make_unique<MediaNotificationAudioDeviceSelectorView>(
      &delegate, service_.get(), "3", gfx::kPlaceholderColor,
      gfx::kPlaceholderColor);

  auto* first_entry =
      view_->audio_device_entries_container_->children().front();
  EXPECT_EQ(EntryLabelText(first_entry), "Earbuds");
  EXPECT_TRUE(IsHighlighted(first_entry));
}

TEST_F(MediaNotificationAudioDeviceSelectorViewTest,
       DeviceHighlightedOnChange) {
  // When the audio output device changes, the UI should highlight that one.
  provider_->AddDevice("Speaker", "1");
  provider_->AddDevice("Headphones", "2");
  provider_->AddDevice("Earbuds", "3");
  service_->set_device_provider_for_testing(std::move(provider_));

  MockMediaNotificationAudioDeviceSelectorViewDelegate delegate;
  view_ = std::make_unique<MediaNotificationAudioDeviceSelectorView>(
      &delegate, service_.get(), "1", gfx::kPlaceholderColor,
      gfx::kPlaceholderColor);

  auto& container_children = view_->audio_device_entries_container_->children();

  // There should be only one highlighted button. It should be the first button.
  // It's text should be "Speaker"
  EXPECT_EQ(util::ranges::count_if(container_children, IsHighlighted), 1);
  EXPECT_EQ(util::ranges::find_if(container_children, IsHighlighted),
            container_children.begin());
  EXPECT_EQ(EntryLabelText(container_children.front()), "Speaker");

  // Simulate a device change
  view_->UpdateCurrentAudioDevice("3");

  // The button for "Earbuds" should come before all others & be highlighted.
  EXPECT_EQ(util::ranges::count_if(container_children, IsHighlighted), 1);
  EXPECT_EQ(util::ranges::find_if(container_children, IsHighlighted),
            container_children.begin());
  EXPECT_EQ(EntryLabelText(container_children.front()), "Earbuds");
}

TEST_F(MediaNotificationAudioDeviceSelectorViewTest, DeviceButtonsChange) {
  // If the device provider reports a change in connect audio devices, the UI
  // should update accordingly.
  provider_->AddDevice("Speaker", "1");
  provider_->AddDevice("Headphones", "2");
  provider_->AddDevice("Earbuds", "3");
  auto* provider = provider_.get();
  service_->set_device_provider_for_testing(std::move(provider_));

  MockMediaNotificationAudioDeviceSelectorViewDelegate delegate;
  view_ = std::make_unique<MediaNotificationAudioDeviceSelectorView>(
      &delegate, service_.get(), "1", gfx::kPlaceholderColor,
      gfx::kPlaceholderColor);

  provider->ResetDevices();
  // Make "Monitor" the default device.
  provider->AddDevice("Monitor",
                      media::AudioDeviceDescription::kDefaultDeviceId);
  provider->RunUICallback();

  auto& container_children = view_->audio_device_entries_container_->children();
  EXPECT_EQ(container_children.size(), 1u);
  ASSERT_FALSE(container_children.empty());
  EXPECT_EQ(EntryLabelText(container_children.front()), "Monitor");

  // When the device highlighted in the UI is removed, the UI should fall back
  // to highlighting the default device.
  EXPECT_TRUE(IsHighlighted(container_children.front()));
}

TEST_F(MediaNotificationAudioDeviceSelectorViewTest, VisibilityChanges) {
  // The audio device selector view should become hidden when there is only one
  // unique device.
  provider_->AddDevice("Speaker", "1");
  provider_->AddDevice("default",
                       media::AudioDeviceDescription::kDefaultDeviceId);
  auto* provider = provider_.get();
  service_->set_device_provider_for_testing(std::move(provider_));

  MockMediaNotificationAudioDeviceSelectorViewDelegate delegate;
  EXPECT_CALL(delegate, OnAudioDeviceSelectorViewSizeChanged).Times(1);
  view_ = std::make_unique<MediaNotificationAudioDeviceSelectorView>(
      &delegate, service_.get(), "1", gfx::kPlaceholderColor,
      gfx::kPlaceholderColor);
  EXPECT_FALSE(view_->GetVisible());

  testing::Mock::VerifyAndClearExpectations(&delegate);

  provider->AddDevice("Headphones", "2");
  EXPECT_CALL(delegate, OnAudioDeviceSelectorViewSizeChanged).Times(1);
  provider->RunUICallback();
  EXPECT_TRUE(view_->GetVisible());
  testing::Mock::VerifyAndClearExpectations(&delegate);
}
