// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_custom_controls_fullscreen_detector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

struct VideoTestParam {
  String description;
  IntRect target_rect;
  bool expected_result;
};

}  // anonymous namespace

class MediaCustomControlsFullscreenDetectorTest : public testing::Test {
 protected:
  void SetUp() override {
    page_holder_ = std::make_unique<DummyPageHolder>();
    new_page_holder_ = std::make_unique<DummyPageHolder>();
  }

  HTMLVideoElement* VideoElement() const {
    return To<HTMLVideoElement>(GetDocument().QuerySelector("video"));
  }

  static MediaCustomControlsFullscreenDetector* FullscreenDetectorFor(
      HTMLVideoElement* video_element) {
    return video_element->custom_controls_fullscreen_detector_;
  }

  MediaCustomControlsFullscreenDetector* FullscreenDetector() const {
    return FullscreenDetectorFor(VideoElement());
  }

  Document& GetDocument() const { return page_holder_->GetDocument(); }
  Document& NewDocument() const { return new_page_holder_->GetDocument(); }

  bool CheckEventListenerRegistered(EventTarget& target,
                                    const AtomicString& event_type,
                                    EventListener* listener) {
    EventListenerVector* listeners = target.GetEventListeners(event_type);
    if (!listeners)
      return false;

    for (const auto& registered_listener : *listeners) {
      if (registered_listener.Callback() == listener)
        return true;
    }
    return false;
  }

 private:
  std::unique_ptr<DummyPageHolder> page_holder_;
  std::unique_ptr<DummyPageHolder> new_page_holder_;
  Persistent<HTMLVideoElement> video_;
};

TEST_F(MediaCustomControlsFullscreenDetectorTest,
       hasNoListenersBeforeAddingToDocument) {
  auto* video = To<HTMLVideoElement>(
      GetDocument().CreateRawElement(html_names::kVideoTag));

  EXPECT_FALSE(CheckEventListenerRegistered(GetDocument(),
                                            event_type_names::kFullscreenchange,
                                            FullscreenDetectorFor(video)));
  EXPECT_FALSE(CheckEventListenerRegistered(
      GetDocument(), event_type_names::kWebkitfullscreenchange,
      FullscreenDetectorFor(video)));
  EXPECT_FALSE(CheckEventListenerRegistered(
      *video, event_type_names::kLoadedmetadata, FullscreenDetectorFor(video)));
}

TEST_F(MediaCustomControlsFullscreenDetectorTest,
       hasListenersAfterAddToDocumentByScript) {
  auto* video = To<HTMLVideoElement>(
      GetDocument().CreateRawElement(html_names::kVideoTag));
  GetDocument().body()->AppendChild(video);

  EXPECT_TRUE(CheckEventListenerRegistered(GetDocument(),
                                           event_type_names::kFullscreenchange,
                                           FullscreenDetector()));
  EXPECT_TRUE(CheckEventListenerRegistered(
      GetDocument(), event_type_names::kWebkitfullscreenchange,
      FullscreenDetector()));
  EXPECT_TRUE(CheckEventListenerRegistered(*VideoElement(),
                                           event_type_names::kLoadedmetadata,
                                           FullscreenDetector()));
}

TEST_F(MediaCustomControlsFullscreenDetectorTest,
       hasListenersAfterAddToDocumentByParser) {
  GetDocument().body()->SetInnerHTMLFromString("<body><video></video></body>");

  EXPECT_TRUE(CheckEventListenerRegistered(GetDocument(),
                                           event_type_names::kFullscreenchange,
                                           FullscreenDetector()));
  EXPECT_TRUE(CheckEventListenerRegistered(
      GetDocument(), event_type_names::kWebkitfullscreenchange,
      FullscreenDetector()));
  EXPECT_TRUE(CheckEventListenerRegistered(*VideoElement(),
                                           event_type_names::kLoadedmetadata,
                                           FullscreenDetector()));
}

TEST_F(MediaCustomControlsFullscreenDetectorTest,
       hasListenersAfterDocumentMove) {
  auto* video = To<HTMLVideoElement>(
      GetDocument().CreateRawElement(html_names::kVideoTag));
  GetDocument().body()->AppendChild(video);

  NewDocument().body()->AppendChild(VideoElement());

  EXPECT_FALSE(CheckEventListenerRegistered(GetDocument(),
                                            event_type_names::kFullscreenchange,
                                            FullscreenDetectorFor(video)));
  EXPECT_FALSE(CheckEventListenerRegistered(
      GetDocument(), event_type_names::kWebkitfullscreenchange,
      FullscreenDetectorFor(video)));

  EXPECT_TRUE(CheckEventListenerRegistered(NewDocument(),
                                           event_type_names::kFullscreenchange,
                                           FullscreenDetectorFor(video)));
  EXPECT_TRUE(CheckEventListenerRegistered(
      NewDocument(), event_type_names::kWebkitfullscreenchange,
      FullscreenDetectorFor(video)));

  EXPECT_TRUE(CheckEventListenerRegistered(
      *video, event_type_names::kLoadedmetadata, FullscreenDetectorFor(video)));
}

}  // namespace blink
