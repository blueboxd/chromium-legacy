// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

#include "components/performance_manager/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/page_live_state_decorator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

void EndToEndPropertyTest(content::WebContents* contents,
                          bool (PageLiveStateDecorator::Data::*pm_getter)()
                              const,
                          void (*ui_thread_setter)(content::WebContents*, bool),
                          bool default_state = false) {
  // By default all properties are set to the default value.
  TestPageLiveStatePropertyOnPMSequence(contents, pm_getter, default_state);

  // Pretend that the property changed and make sure that the PageNode data gets
  // updated.
  (*ui_thread_setter)(contents, !default_state);
  TestPageLiveStatePropertyOnPMSequence(contents, pm_getter, !default_state);

  // Switch back to the default state.
  (*ui_thread_setter)(contents, default_state);
  TestPageLiveStatePropertyOnPMSequence(contents, pm_getter, default_state);
}

}  // namespace

class PageLiveStateDecoratorTest : public PerformanceManagerTestHarness {
 protected:
  PageLiveStateDecoratorTest() = default;
  ~PageLiveStateDecoratorTest() override = default;
  PageLiveStateDecoratorTest(const PageLiveStateDecoratorTest& other) = delete;
  PageLiveStateDecoratorTest& operator=(const PageLiveStateDecoratorTest&) =
      delete;

  void SetUp() override {
    PerformanceManagerTestHarness::SetUp();
    auto contents = CreateTestWebContents();
    contents_ = contents.get();
    SetContents(std::move(contents));
  }

  void TearDown() override {
    DeleteContents();
    contents_ = nullptr;
    PerformanceManagerTestHarness::TearDown();
  }

  content::WebContents* contents() { return contents_; }

 private:
  content::WebContents* contents_ = nullptr;
};

TEST_F(PageLiveStateDecoratorTest, OnIsConnectedToUSBDeviceChanged) {
  EndToEndPropertyTest(
      contents(), &PageLiveStateDecorator::Data::IsConnectedToUSBDevice,
      &PageLiveStateDecorator::OnIsConnectedToUSBDeviceChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsConnectedToBluetoothDeviceChanged) {
  EndToEndPropertyTest(
      contents(), &PageLiveStateDecorator::Data::IsConnectedToBluetoothDevice,
      &PageLiveStateDecorator::OnIsConnectedToBluetoothDeviceChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingVideoChanged) {
  EndToEndPropertyTest(contents(),
                       &PageLiveStateDecorator::Data::IsCapturingVideo,
                       &PageLiveStateDecorator::OnIsCapturingVideoChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingAudioChanged) {
  EndToEndPropertyTest(contents(),
                       &PageLiveStateDecorator::Data::IsCapturingAudio,
                       &PageLiveStateDecorator::OnIsCapturingAudioChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsBeingMirroredChanged) {
  EndToEndPropertyTest(contents(),
                       &PageLiveStateDecorator::Data::IsBeingMirrored,
                       &PageLiveStateDecorator::OnIsBeingMirroredChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingDesktopChanged) {
  EndToEndPropertyTest(contents(),
                       &PageLiveStateDecorator::Data::IsCapturingDesktop,
                       &PageLiveStateDecorator::OnIsCapturingDesktopChanged);
}

TEST_F(PageLiveStateDecoratorTest, SetIsAutoDiscardable) {
  EndToEndPropertyTest(contents(),
                       &PageLiveStateDecorator::Data::IsAutoDiscardable,
                       &PageLiveStateDecorator::SetIsAutoDiscardable,
                       /*default_state=*/true);
}

}  // namespace performance_manager
