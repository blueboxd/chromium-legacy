// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/fallback_cursor_event_manager.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class FallbackCursorChromeClient : public RenderingTestChromeClient {
 public:
  FallbackCursorChromeClient() {}

  void FallbackCursorModeLockCursor(LocalFrame* frame,
                                    bool left,
                                    bool right,
                                    bool up,
                                    bool down) override {
    cursor_lock_[0] = left;
    cursor_lock_[1] = right;
    cursor_lock_[2] = up;
    cursor_lock_[3] = down;
  }

  void FallbackCursorModeSetCursorVisibility(LocalFrame* frame,
                                             bool visible) override {
    cursor_visible_ = visible;
  }

  bool cursor_lock_[4] = {0};
  bool cursor_visible_ = true;

 private:
  DISALLOW_COPY_AND_ASSIGN(FallbackCursorChromeClient);
};

class FallbackCursorEventManagerTest : public RenderingTest,
                                       private ScopedFallbackCursorModeForTest {
 protected:
  FallbackCursorEventManagerTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()),
        ScopedFallbackCursorModeForTest(true),
        chrome_client_(MakeGarbageCollected<FallbackCursorChromeClient>()) {}

  ~FallbackCursorEventManagerTest() override {}

  RenderingTestChromeClient& GetChromeClient() const override {
    return *chrome_client_;
  }

  FallbackCursorChromeClient& GetFallbackCursorChromeClient() const {
    return *chrome_client_;
  }

  void TurnOnFallbackCursorMode() {
    GetDocument().GetFrame()->GetEventHandler().SetIsFallbackCursorModeOn(true);
  }

  void MouseMove(int x, int y, float scale = 1.0f) {
    WebMouseEvent event(WebInputEvent::kMouseMove, WebFloatPoint(x, y),
                        WebFloatPoint(x, y),
                        WebPointerProperties::Button::kNoButton, 0,
                        WebInputEvent::kNoModifiers, CurrentTimeTicks());
    event.SetFrameScale(scale);
    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
        event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  }

  void MouseDown(int x, int y) {
    WebMouseEvent event(
        WebInputEvent::kMouseDown, WebFloatPoint(x, y), WebFloatPoint(x, y),
        WebPointerProperties::Button::kLeft, 0,
        WebInputEvent::Modifiers::kLeftButtonDown, CurrentTimeTicks());
    event.SetFrameScale(1);
    GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(event);
  }

  bool KeyBack() {
    return GetDocument()
        .GetFrame()
        ->GetEventHandler()
        .HandleFallbackCursorModeBackEvent();
  }

  void ExpectLock(bool l, bool r, bool u, bool d) {
    EXPECT_EQ(GetFallbackCursorChromeClient().cursor_lock_[0], l);
    EXPECT_EQ(GetFallbackCursorChromeClient().cursor_lock_[1], r);
    EXPECT_EQ(GetFallbackCursorChromeClient().cursor_lock_[2], u);
    EXPECT_EQ(GetFallbackCursorChromeClient().cursor_lock_[3], d);
  }

 private:
  Persistent<FallbackCursorChromeClient> chrome_client_;

  DISALLOW_COPY_AND_ASSIGN(FallbackCursorEventManagerTest);
};

TEST_F(FallbackCursorEventManagerTest, RootFrameNotScrollable) {
  SetBodyInnerHTML("A");
  TurnOnFallbackCursorMode();

  // Mouse move to edge.
  MouseMove(0, 0);
  ExpectLock(false, false, false, false);

  MouseMove(0, 600);
  ExpectLock(false, false, false, false);

  MouseMove(800, 0);
  ExpectLock(false, false, false, false);

  MouseMove(800, 600);
  ExpectLock(false, false, false, false);
}

TEST_F(FallbackCursorEventManagerTest, MouseMoveCursorLockOnRootFrame) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    .big {
      height: 10000px;
      width: 10000px;
    }
    </style>
    <div class='big'></div>
  )HTML");
  TurnOnFallbackCursorMode();

  // Move below the scroll down line.
  MouseMove(100, 500);
  ExpectLock(false, false, false, true);

  // Move above the scroll down line.
  MouseMove(100, 400);
  ExpectLock(false, false, false, false);

  // Move to the right of scroll right line.
  MouseMove(600, 400);
  ExpectLock(false, true, false, false);
}

TEST_F(FallbackCursorEventManagerTest,
       MouseMoveCursorLockOnRootFrameWithScale) {
  const float SCALE = 0.5f;
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    .big {
      height: 10000px;
      width: 10000px;
    }
    </style>
    <div class='big'></div>
  )HTML");
  TurnOnFallbackCursorMode();

  // Move below the scroll down line.
  MouseMove(50, 250, SCALE);
  ExpectLock(false, false, false, true);

  // Move above the scroll down line.
  MouseMove(50, 200, SCALE);
  ExpectLock(false, false, false, false);

  // Move to the right of scroll right line.
  MouseMove(300, 200, SCALE);
  ExpectLock(false, true, false, false);
}

TEST_F(FallbackCursorEventManagerTest, MouseMoveCursorLockOnDiv) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    .big {
      height: 10000px;
      width: 10000px;
    }
    #d1 {
      height: 100px;
      width: 100px;
      overflow: auto;
    }
    </style>
    <div id='d1'>
      <div class='big'></div>
    </div>
  )HTML");
  TurnOnFallbackCursorMode();

  // Move below the scroll down line but before mouse down.
  MouseMove(50, 80);
  ExpectLock(false, false, false, false);
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);

  // Mouse down and move lock on down.
  MouseDown(50, 80);
  Element* d1 = GetDocument().getElementById("d1");
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .fallback_cursor_event_manager_->current_node_.Get(),
            d1);
  MouseMove(50, 80);
  ExpectLock(false, false, false, true);

  // Mouse move out of div.
  MouseMove(200, 200);
  ExpectLock(false, false, false, false);
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);

  // key back.
  MouseMove(50, 80);
  MouseDown(50, 80);
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .fallback_cursor_event_manager_->current_node_.Get(),
            d1);
  EXPECT_TRUE(KeyBack());
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);
}

TEST_F(FallbackCursorEventManagerTest, MouseMoveCursorLockOnIFrame) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    #ifr {
      height: 100px;
      width: 100px;
    }
    </style>
    <iframe id='ifr'></iframe>
  )HTML");

  SetChildFrameHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    .big {
      height: 10000px;
      width: 10000px;
    }
    </style>
    <div class='big'></div>
  )HTML");
  TurnOnFallbackCursorMode();

  // Move below the scroll down line but before mouse down.
  MouseMove(50, 80);
  ExpectLock(false, false, false, false);
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);

  // Mouse down and move lock on down.
  MouseDown(50, 80);
  MouseMove(50, 80);
  ExpectLock(false, false, false, true);
  Node* child_frame_doc = ChildFrame().GetDocument();
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .fallback_cursor_event_manager_->current_node_.Get(),
            child_frame_doc);

  // Mouse move out of iframe.
  MouseMove(200, 200);
  ExpectLock(false, false, false, false);
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);

  // key back.
  MouseMove(50, 80);
  MouseDown(50, 80);
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .fallback_cursor_event_manager_->current_node_.Get(),
            child_frame_doc);
  EXPECT_TRUE(KeyBack());
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);
}

TEST_F(FallbackCursorEventManagerTest, KeyBackAndMouseMove) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    #ifr {
      height: 100px;
      width: 100px;
    }
    div {
      height: 10000px;
      width: 10000px;
    }
    </style>
    <iframe id='ifr'></iframe>
    <div></div>
  )HTML");

  SetChildFrameHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    .big {
      height: 10000px;
      width: 10000px;
    }
    </style>
    <div class='big'></div>
  )HTML");
  TurnOnFallbackCursorMode();

  // Move below the scroll down line but before mouse down.
  MouseMove(50, 80);
  ExpectLock(false, false, false, false);
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);

  // Mouse down and move lock on down.
  MouseDown(50, 80);
  MouseMove(50, 80);
  ExpectLock(false, false, false, true);
  Node* child_frame_doc = ChildFrame().GetDocument();
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .fallback_cursor_event_manager_->current_node_.Get(),
            child_frame_doc);

  // key back.
  EXPECT_TRUE(KeyBack());
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);

  // Move below the scroll down line of page.
  MouseMove(100, 500);
  ExpectLock(false, false, false, true);
}

TEST_F(FallbackCursorEventManagerTest, MouseDownOnEditor) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    #editor {
      height: 100px;
      width: 100px;
    }
    </style>
    <div id='editor' contenteditable='true'>
    </div>
  )HTML");
  TurnOnFallbackCursorMode();

  MouseMove(50, 80);
  MouseDown(50, 80);

  EXPECT_EQ(GetFallbackCursorChromeClient().cursor_visible_, false);

  Element* editor = GetDocument().getElementById("editor");
  EXPECT_EQ(GetDocument().FocusedElement(), editor);

  EXPECT_TRUE(KeyBack());

  EXPECT_EQ(GetFallbackCursorChromeClient().cursor_visible_, true);
  EXPECT_FALSE(GetDocument().FocusedElement());
}

// Ensure the cursor causes correct locking and scrolling when the web page is
// zoomed in and the visual viewport is offset.
TEST_F(FallbackCursorEventManagerTest, ZoomedIn) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    .big {
      height: 10000px;
      width: 10000px;
    }
    </style>
    <div class='big'></div>
  )HTML");
  TurnOnFallbackCursorMode();
  VisualViewport& visual_viewport =
      GetDocument().GetPage()->GetVisualViewport();
  visual_viewport.SetScaleAndLocation(4, /*is_pinch_gesture_active=*/false,
                                      FloatPoint(400, 300));

  ASSERT_EQ(IntSize(800, 600), GetDocument().View()->Size());
  ASSERT_EQ(FloatSize(200, 150), visual_viewport.VisibleRect().Size());

  // Note: MouseMove position is specified in root frame coordinates. To
  // determine the coordinates in the visual viewport, subtract the visual
  // viewport offset (400, 300) from the MouseMove position.

  // Move to the center of the viewport.
  MouseMove(500, 375);
  ExpectLock(false, false, false, false);

  // Move below the scroll down line.
  MouseMove(500, 430);
  ExpectLock(false, false, false, true);

  // Move to the left of scroll left line.
  MouseMove(450, 375);
  ExpectLock(true, false, false, false);
}

TEST_F(FallbackCursorEventManagerTest, NotInCursorMode) {
  GetPage().SetIsCursorVisible(false);
  EXPECT_FALSE(KeyBack());
}

}  // namespace blink
