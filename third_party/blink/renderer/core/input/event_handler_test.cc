// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/event_handler.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/dom_selection.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class EventHandlerTest : public PageTestBase {
 protected:
  void SetUp() override;
  void SetHtmlInnerHTML(const char* html_content);
  ShadowRoot* SetShadowContent(const char* shadow_content, const char* host);
};

class EventHandlerSimTest : public SimTest {};

class TapEventBuilder : public WebGestureEvent {
 public:
  TapEventBuilder(FloatPoint position, int tap_count)
      : WebGestureEvent(WebInputEvent::kGestureTap,
                        WebInputEvent::kNoModifiers,
                        CurrentTimeTicks(),
                        WebGestureDevice::kTouchscreen) {
    SetPositionInWidget(position);
    SetPositionInScreen(position);
    data.tap.tap_count = tap_count;
    data.tap.width = 5;
    data.tap.height = 5;
    frame_scale_ = 1;
  }
};

class TapDownEventBuilder : public WebGestureEvent {
 public:
  TapDownEventBuilder(FloatPoint position)
      : WebGestureEvent(WebInputEvent::kGestureTapDown,
                        WebInputEvent::kNoModifiers,
                        CurrentTimeTicks(),
                        WebGestureDevice::kTouchscreen) {
    SetPositionInWidget(position);
    SetPositionInScreen(position);
    data.tap_down.width = 5;
    data.tap_down.height = 5;
    frame_scale_ = 1;
  }
};

class ShowPressEventBuilder : public WebGestureEvent {
 public:
  ShowPressEventBuilder(FloatPoint position)
      : WebGestureEvent(WebInputEvent::kGestureShowPress,
                        WebInputEvent::kNoModifiers,
                        CurrentTimeTicks(),
                        WebGestureDevice::kTouchscreen) {
    SetPositionInWidget(position);
    SetPositionInScreen(position);
    data.show_press.width = 5;
    data.show_press.height = 5;
    frame_scale_ = 1;
  }
};

class LongPressEventBuilder : public WebGestureEvent {
 public:
  LongPressEventBuilder(FloatPoint position)
      : WebGestureEvent(WebInputEvent::kGestureLongPress,
                        WebInputEvent::kNoModifiers,
                        CurrentTimeTicks(),
                        WebGestureDevice::kTouchscreen) {
    SetPositionInWidget(position);
    SetPositionInScreen(position);
    data.long_press.width = 5;
    data.long_press.height = 5;
    frame_scale_ = 1;
  }
};

class MousePressEventBuilder : public WebMouseEvent {
 public:
  MousePressEventBuilder(IntPoint position_param,
                         int click_count_param,
                         WebMouseEvent::Button button_param)
      : WebMouseEvent(WebInputEvent::kMouseDown,
                      WebInputEvent::kNoModifiers,
                      CurrentTimeTicks()) {
    click_count = click_count_param;
    button = button_param;
    SetPositionInWidget(position_param.X(), position_param.Y());
    SetPositionInScreen(position_param.X(), position_param.Y());
    frame_scale_ = 1;
  }
};

void EventHandlerTest::SetUp() {
  PageTestBase::SetUp(IntSize(300, 400));
}

void EventHandlerTest::SetHtmlInnerHTML(const char* html_content) {
  GetDocument().documentElement()->SetInnerHTMLFromString(
      String::FromUTF8(html_content));
  UpdateAllLifecyclePhasesForTest();
}

ShadowRoot* EventHandlerTest::SetShadowContent(const char* shadow_content,
                                               const char* host) {
  ShadowRoot* shadow_root =
      EditingTestBase::CreateShadowRootForElementWithIDAndSetInnerHTML(
          GetDocument(), host, shadow_content);
  return shadow_root;
}

TEST_F(EventHandlerTest, dragSelectionAfterScroll) {
  SetHtmlInnerHTML(
      "<style> body { margin: 0px; } .upper { width: 300px; height: 400px; }"
      ".lower { margin: 0px; width: 300px; height: 400px; } .line { display: "
      "block; width: 300px; height: 30px; } </style>"
      "<div class='upper'></div>"
      "<div class='lower'>"
      "<span class='line'>Line 1</span><span class='line'>Line 2</span><span "
      "class='line'>Line 3</span><span class='line'>Line 4</span><span "
      "class='line'>Line 5</span>"
      "<span class='line'>Line 6</span><span class='line'>Line 7</span><span "
      "class='line'>Line 8</span><span class='line'>Line 9</span><span "
      "class='line'>Line 10</span>"
      "</div>");

  LocalFrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewport()->SetScrollOffset(ScrollOffset(0, 400),
                                                kProgrammaticScroll);

  WebMouseEvent mouse_down_event(WebInputEvent::kMouseDown, WebFloatPoint(0, 0),
                                 WebFloatPoint(100, 200),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  ASSERT_TRUE(GetDocument()
                  .GetFrame()
                  ->GetEventHandler()
                  .GetSelectionController()
                  .MouseDownMayStartSelect());

  WebMouseEvent mouse_move_event(
      WebInputEvent::kMouseMove, WebFloatPoint(100, 50),
      WebFloatPoint(200, 250), WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown,
      WebInputEvent::GetStaticTimeStampForTests());
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  GetPage().GetAutoscrollController().Animate();
  GetPage().Animator().ServiceScriptedAnimations(WTF::CurrentTimeTicks());

  WebMouseEvent mouse_up_event(
      WebMouseEvent::kMouseUp, WebFloatPoint(100, 50), WebFloatPoint(200, 250),
      WebPointerProperties::Button::kLeft, 1, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  mouse_up_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_up_event);

  ASSERT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .GetSelectionController()
                   .MouseDownMayStartSelect());

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  Range* range =
      CreateRange(EphemeralRange(Selection().GetSelectionInDOMTree().Base(),
                                 Selection().GetSelectionInDOMTree().Extent()));
  ASSERT_TRUE(range);
  EXPECT_EQ("Line 1\nLine 2", range->GetText());
}

TEST_F(EventHandlerTest, multiClickSelectionFromTap) {
  SetHtmlInnerHTML(
      "<style> body { margin: 0px; } .line { display: block; width: 300px; "
      "height: 30px; } </style>"
      "<body contenteditable='true'><span class='line' id='line'>One Two "
      "Three</span></body>");

  Node* line = GetDocument().getElementById("line")->firstChild();

  TapEventBuilder single_tap_event(FloatPoint(0, 0), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Base());

  // Multi-tap events on editable elements should trigger selection, just
  // like multi-click events.
  TapEventBuilder double_tap_event(FloatPoint(0, 0), 2);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      double_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Base());
  if (GetDocument()
          .GetFrame()
          ->GetEditor()
          .IsSelectTrailingWhitespaceEnabled()) {
    EXPECT_EQ(Position(line, 4), Selection().GetSelectionInDOMTree().Extent());
    EXPECT_EQ("One ", Selection().SelectedText().Utf8());
  } else {
    EXPECT_EQ(Position(line, 3), Selection().GetSelectionInDOMTree().Extent());
    EXPECT_EQ("One", Selection().SelectedText().Utf8());
  }

  TapEventBuilder triple_tap_event(FloatPoint(0, 0), 3);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      triple_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Base());
  EXPECT_EQ(Position(line, 13), Selection().GetSelectionInDOMTree().Extent());
  EXPECT_EQ("One Two Three", Selection().SelectedText().Utf8());
}

TEST_F(EventHandlerTest, multiClickSelectionFromTapDisabledIfNotEditable) {
  SetHtmlInnerHTML(
      "<style> body { margin: 0px; } .line { display: block; width: 300px; "
      "height: 30px; } </style>"
      "<span class='line' id='line'>One Two Three</span>");

  Node* line = GetDocument().getElementById("line")->firstChild();

  TapEventBuilder single_tap_event(FloatPoint(0, 0), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Base());

  // As the text is readonly, multi-tap events should not trigger selection.
  TapEventBuilder double_tap_event(FloatPoint(0, 0), 2);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      double_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Base());

  TapEventBuilder triple_tap_event(FloatPoint(0, 0), 3);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      triple_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Base());
}

TEST_F(EventHandlerTest, draggedInlinePositionTest) {
  SetHtmlInnerHTML(
      "<style>"
      "body { margin: 0px; }"
      ".line { font-family: sans-serif; background: blue; width: 300px; "
      "height: 30px; font-size: 40px; margin-left: 250px; }"
      "</style>"
      "<div style='width: 300px; height: 100px;'>"
      "<span class='line' draggable='true'>abcd</span>"
      "</div>");
  WebMouseEvent mouse_down_event(WebMouseEvent::kMouseDown,
                                 WebFloatPoint(262, 29), WebFloatPoint(329, 67),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent mouse_move_event(
      WebMouseEvent::kMouseMove, WebFloatPoint(618, 298),
      WebFloatPoint(685, 436), WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown,
      WebInputEvent::GetStaticTimeStampForTests());
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  EXPECT_EQ(IntPoint(12, 29), GetDocument()
                                  .GetFrame()
                                  ->GetEventHandler()
                                  .DragDataTransferLocationForTesting());
}

TEST_F(EventHandlerTest, draggedSVGImagePositionTest) {
  SetHtmlInnerHTML(
      "<style>"
      "body { margin: 0px; }"
      "[draggable] {"
      "-webkit-user-select: none; user-select: none; -webkit-user-drag: "
      "element; }"
      "</style>"
      "<div style='width: 300px; height: 100px;'>"
      "<svg width='500' height='500'>"
      "<rect x='100' y='100' width='100px' height='100px' fill='blue' "
      "draggable='true'/>"
      "</svg>"
      "</div>");
  WebMouseEvent mouse_down_event(
      WebMouseEvent::kMouseDown, WebFloatPoint(145, 144),
      WebFloatPoint(212, 282), WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown,
      WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent mouse_move_event(
      WebMouseEvent::kMouseMove, WebFloatPoint(618, 298),
      WebFloatPoint(685, 436), WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown,
      WebInputEvent::GetStaticTimeStampForTests());
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  EXPECT_EQ(IntPoint(45, 44), GetDocument()
                                  .GetFrame()
                                  ->GetEventHandler()
                                  .DragDataTransferLocationForTesting());
}

TEST_F(EventHandlerTest, HitOnNothingDoesNotShowIBeam) {
  SetHtmlInnerHTML("");
  HitTestLocation location((PhysicalOffset(10, 10)));
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(
          GetDocument().body(), hit));
}

TEST_F(EventHandlerTest, HitOnTextShowsIBeam) {
  SetHtmlInnerHTML("blabla");
  Node* const text = GetDocument().body()->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().Center());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, HitOnUserSelectNoneDoesNotShowIBeam) {
  SetHtmlInnerHTML("<span style='user-select: none'>blabla</span>");
  Node* const text = GetDocument().body()->firstChild()->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().Center());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_FALSE(text->CanStartSelection());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, ShadowChildCanOverrideUserSelectNone) {
  SetHtmlInnerHTML("<p style='user-select: none' id='host'></p>");
  ShadowRoot* const shadow_root = SetShadowContent(
      "<span style='user-select: text' id='bla'>blabla</span>", "host");

  Node* const text = shadow_root->getElementById("bla")->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().Center());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, UserSelectAllCanOverrideUserSelectNone) {
  SetHtmlInnerHTML(
      "<div style='user-select: none'>"
      "<span style='user-select: all'>blabla</span>"
      "</div>");
  Node* const text =
      GetDocument().body()->firstChild()->firstChild()->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().Center());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, UserSelectNoneCanOverrideUserSelectAll) {
  SetHtmlInnerHTML(
      "<div style='user-select: all'>"
      "<span style='user-select: none'>blabla</span>"
      "</div>");
  Node* const text =
      GetDocument().body()->firstChild()->firstChild()->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().Center());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_FALSE(text->CanStartSelection());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, UserSelectTextCanOverrideUserSelectNone) {
  SetHtmlInnerHTML(
      "<div style='user-select: none'>"
      "<span style='user-select: text'>blabla</span>"
      "</div>");
  Node* const text =
      GetDocument().body()->firstChild()->firstChild()->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().Center());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, UserSelectNoneCanOverrideUserSelectText) {
  SetHtmlInnerHTML(
      "<div style='user-select: text'>"
      "<span style='user-select: none'>blabla</span>"
      "</div>");
  Node* const text = GetDocument().body()->firstChild()->firstChild()->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().Center());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_FALSE(text->CanStartSelection());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, ShadowChildCanOverrideUserSelectText) {
  SetHtmlInnerHTML("<p style='user-select: text' id='host'></p>");
  ShadowRoot* const shadow_root = SetShadowContent(
      "<span style='user-select: none' id='bla'>blabla</span>", "host");

  Node* const text = shadow_root->getElementById("bla")->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().Center());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_FALSE(text->CanStartSelection());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, InputFieldsCanStartSelection) {
  SetHtmlInnerHTML("<input value='blabla'>");
  auto* const field = ToHTMLInputElement(GetDocument().body()->firstChild());
  Element* const text = field->InnerEditorElement();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().Center());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, ReadOnlyInputDoesNotInheritUserSelect) {
  SetHtmlInnerHTML(
      "<div style='user-select: none'>"
      "<input id='sample' readonly value='blabla'>"
      "</div>");
  HTMLInputElement* const input =
      ToHTMLInputElement(GetDocument().getElementById("sample"));
  Node* const text = input->InnerEditorElement()->firstChild();

  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().Center());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, ImagesCannotStartSelection) {
  SetHtmlInnerHTML("<img>");
  auto* const img = To<Element>(GetDocument().body()->firstChild());
  HitTestLocation location(
      img->GetLayoutObject()->AbsoluteBoundingBoxRect().Center());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_FALSE(img->CanStartSelection());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(img,
                                                                         hit));
}

TEST_F(EventHandlerTest, AnchorTextCannotStartSelection) {
  SetHtmlInnerHTML("<a href='bala'>link text</a>");
  Node* const link = GetDocument().body()->firstChild();
  HitTestLocation location(
      link->GetLayoutObject()->AbsoluteBoundingBoxRect().Center());
  HitTestResult result =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  Node* const text = link->firstChild();
  EXPECT_FALSE(text->CanStartSelection());
  EXPECT_TRUE(result.IsOverLink());
  // ShouldShowIBeamForNode() returns |cursor: auto|'s value.
  // In https://github.com/w3c/csswg-drafts/issues/1598 it was decided that:
  // a { cursor: auto } /* gives I-beam over links */
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(
          text, result));
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .SelectCursor(location, result)
                .GetCursor()
                .GetType(),
            Cursor::Type::kHand);  // A hand signals ability to navigate.
}

TEST_F(EventHandlerTest, EditableAnchorTextCanStartSelection) {
  SetHtmlInnerHTML("<a contenteditable='true' href='bala'>editable link</a>");
  Node* const link = GetDocument().body()->firstChild();
  HitTestLocation location(
      link->GetLayoutObject()->AbsoluteBoundingBoxRect().Center());
  HitTestResult result =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  Node* const text = link->firstChild();
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(result.IsOverLink());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(
          text, result));
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .SelectCursor(location, result)
                .GetCursor()
                .GetType(),
            Cursor::Type::kIBeam);  // An I-beam signals editability.
}

// Regression test for http://crbug.com/641403 to verify we use up-to-date
// layout tree for dispatching "contextmenu" event.
TEST_F(EventHandlerTest, sendContextMenuEventWithHover) {
  SetHtmlInnerHTML(
      "<style>*:hover { color: red; }</style>"
      "<div>foo</div>");
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->SetInnerHTMLFromString(
      "document.addEventListener('contextmenu', event => "
      "event.preventDefault());");
  GetDocument().body()->AppendChild(script);
  GetDocument().UpdateStyleAndLayout();
  GetDocument().GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(GetDocument().body(), 0))
          .Build());
  WebMouseEvent mouse_down_event(
      WebMouseEvent::kMouseDown, WebFloatPoint(0, 0), WebFloatPoint(100, 200),
      WebPointerProperties::Button::kRight, 1,
      WebInputEvent::Modifiers::kRightButtonDown, CurrentTimeTicks());
  mouse_down_event.SetFrameScale(1);
  EXPECT_EQ(WebInputEventResult::kHandledApplication,
            GetDocument().GetFrame()->GetEventHandler().SendContextMenuEvent(
                mouse_down_event));
}

TEST_F(EventHandlerTest, EmptyTextfieldInsertionOnTap) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50></textarea>");

  TapEventBuilder single_tap_event(FloatPoint(200, 200), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, NonEmptyTextfieldInsertionOnTap) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50>Enter text</textarea>");

  TapEventBuilder single_tap_event(FloatPoint(200, 200), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, NewlineDivInsertionOnTap) {
  SetHtmlInnerHTML("<div contenteditable><br/></div>");

  TapEventBuilder single_tap_event(FloatPoint(10, 10), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, EmptyTextfieldInsertionOnLongPress) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50></textarea>");

  LongPressEventBuilder long_press_event(FloatPoint(200, 200));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      long_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());

  // Single Tap on an empty edit field should clear insertion handle
  TapEventBuilder single_tap_event(FloatPoint(200, 200), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, NonEmptyTextfieldInsertionOnLongPress) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50>Enter text</textarea>");

  LongPressEventBuilder long_press_event(FloatPoint(200, 200));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      long_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, ClearHandleAfterTap) {
  SetHtmlInnerHTML("<textarea cols=50  rows=10>Enter text</textarea>");

  // Show handle
  LongPressEventBuilder long_press_event(FloatPoint(200, 10));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      long_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());

  // Tap away from text area should clear handle
  TapEventBuilder single_tap_event(FloatPoint(200, 350), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_FALSE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, HandleNotShownOnMouseEvents) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50>Enter text</textarea>");

  MousePressEventBuilder left_mouse_press_event(
      IntPoint(200, 200), 1, WebPointerProperties::Button::kLeft);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      left_mouse_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());

  MousePressEventBuilder right_mouse_press_event(
      IntPoint(200, 200), 1, WebPointerProperties::Button::kRight);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      right_mouse_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());

  MousePressEventBuilder double_click_mouse_press_event(
      IntPoint(200, 200), 2, WebPointerProperties::Button::kLeft);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      double_click_mouse_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  ASSERT_FALSE(Selection().IsHandleVisible());

  MousePressEventBuilder triple_click_mouse_press_event(
      IntPoint(200, 200), 3, WebPointerProperties::Button::kLeft);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      triple_click_mouse_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  ASSERT_FALSE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, MisspellingContextMenuEvent) {
  if (GetDocument()
          .GetFrame()
          ->GetEditor()
          .Behavior()
          .ShouldSelectOnContextualMenuClick())
    return;

  SetHtmlInnerHTML("<textarea cols=50 rows=50>Mispellinggg</textarea>");

  TapEventBuilder single_tap_event(FloatPoint(10, 10), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());

  GetDocument().GetFrame()->GetEventHandler().ShowNonLocatedContextMenu(
      nullptr, kMenuSourceTouchHandle);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, dragEndInNewDrag) {
  SetHtmlInnerHTML(
      "<style>.box { width: 100px; height: 100px; display: block; }</style>"
      "<a class='box' href=''>Drag me</a>");

  WebMouseEvent mouse_down_event(
      WebInputEvent::kMouseDown, WebFloatPoint(50, 50), WebFloatPoint(50, 50),
      WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown, CurrentTimeTicks());
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent mouse_move_event(
      WebInputEvent::kMouseMove, WebFloatPoint(51, 50), WebFloatPoint(51, 50),
      WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown, CurrentTimeTicks());
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  // This reproduces what might be the conditions of http://crbug.com/677916
  //
  // TODO(crbug.com/682047): The call sequence below should not occur outside
  // this contrived test. Given the current code, it is unclear how the
  // dragSourceEndedAt() call could occur before a drag operation is started.

  WebMouseEvent mouse_up_event(WebInputEvent::kMouseUp, WebFloatPoint(100, 50),
                               WebFloatPoint(200, 250),
                               WebPointerProperties::Button::kLeft, 1,
                               WebInputEvent::kNoModifiers, CurrentTimeTicks());
  mouse_up_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().DragSourceEndedAt(
      mouse_up_event, kDragOperationNone);

  // This test passes if it doesn't crash.
}

// This test mouse move with modifier kRelativeMotionEvent
// should not start drag.
TEST_F(EventHandlerTest, FakeMouseMoveNotStartDrag) {
  SetHtmlInnerHTML(
      "<style>"
      "body { margin: 0px; }"
      ".line { font-family: sans-serif; background: blue; width: 300px; "
      "height: 30px; font-size: 40px; margin-left: 250px; }"
      "</style>"
      "<div style='width: 300px; height: 100px;'>"
      "<span class='line' draggable='true'>abcd</span>"
      "</div>");
  WebMouseEvent mouse_down_event(WebMouseEvent::kMouseDown,
                                 WebFloatPoint(262, 29), WebFloatPoint(329, 67),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent fake_mouse_move(
      WebMouseEvent::kMouseMove, WebFloatPoint(618, 298),
      WebFloatPoint(685, 436), WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown |
          WebInputEvent::Modifiers::kRelativeMotionEvent,
      WebInputEvent::GetStaticTimeStampForTests());
  fake_mouse_move.SetFrameScale(1);
  EXPECT_EQ(
      WebInputEventResult::kHandledSuppressed,
      GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
          fake_mouse_move, Vector<WebMouseEvent>(), Vector<WebMouseEvent>()));

  EXPECT_EQ(IntPoint(0, 0), GetDocument()
                                .GetFrame()
                                ->GetEventHandler()
                                .DragDataTransferLocationForTesting());
}

class TooltipCapturingChromeClient : public EmptyChromeClient {
 public:
  TooltipCapturingChromeClient() = default;

  void SetToolTip(LocalFrame&, const String& str, TextDirection) override {
    last_tool_tip_ = str;
  }

  String& LastToolTip() { return last_tool_tip_; }

 private:
  String last_tool_tip_;
};

class EventHandlerTooltipTest : public EventHandlerTest {
 public:
  EventHandlerTooltipTest() = default;

  void SetUp() override {
    chrome_client_ = MakeGarbageCollected<TooltipCapturingChromeClient>();
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();
    SetupPageWithClients(&clients);
  }

  String& LastToolTip() { return chrome_client_->LastToolTip(); }

 private:
  Persistent<TooltipCapturingChromeClient> chrome_client_;
};

TEST_F(EventHandlerTooltipTest, mouseLeaveClearsTooltip) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetHtmlInnerHTML(
      "<style>.box { width: 100%; height: 100%; }</style>"
      "<img src='image.png' class='box' title='tooltip'>link</img>");

  EXPECT_EQ(WTF::String(), LastToolTip());

  WebMouseEvent mouse_move_event(
      WebInputEvent::kMouseMove, WebFloatPoint(51, 50), WebFloatPoint(51, 50),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      CurrentTimeTicks());
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  EXPECT_EQ("tooltip", LastToolTip());

  WebMouseEvent mouse_leave_event(
      WebInputEvent::kMouseLeave, WebFloatPoint(0, 0), WebFloatPoint(0, 0),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      CurrentTimeTicks());
  mouse_leave_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseLeaveEvent(
      mouse_leave_event);

  EXPECT_EQ(WTF::String(), LastToolTip());
}

class UnbufferedInputEventsTrackingChromeClient : public EmptyChromeClient {
 public:
  UnbufferedInputEventsTrackingChromeClient() = default;

  void RequestUnbufferedInputEvents(LocalFrame*) override {
    received_unbuffered_request_ = true;
  }

  bool ReceivedRequestForUnbufferedInput() {
    bool value = received_unbuffered_request_;
    received_unbuffered_request_ = false;
    return value;
  }

 private:
  bool received_unbuffered_request_ = false;
};

class EventHandlerLatencyTest : public PageTestBase {
 protected:
  void SetUp() override {
    chrome_client_ =
        MakeGarbageCollected<UnbufferedInputEventsTrackingChromeClient>();
    Page::PageClients page_clients;
    FillWithEmptyClients(page_clients);
    page_clients.chrome_client = chrome_client_.Get();
    SetupPageWithClients(&page_clients);
  }

  void SetHtmlInnerHTML(const char* html_content) {
    GetDocument().documentElement()->SetInnerHTMLFromString(
        String::FromUTF8(html_content));
    UpdateAllLifecyclePhasesForTest();
  }

  Persistent<UnbufferedInputEventsTrackingChromeClient> chrome_client_;
};

TEST_F(EventHandlerLatencyTest, NeedsUnbufferedInput) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetHtmlInnerHTML(
      "<canvas style='width: 100px; height: 100px' id='first' "
      "onpointermove='return;'>");

  HTMLCanvasElement& canvas =
      ToHTMLCanvasElement(*GetDocument().getElementById("first"));

  ASSERT_FALSE(chrome_client_->ReceivedRequestForUnbufferedInput());

  WebMouseEvent mouse_press_event(
      WebInputEvent::kMouseDown, WebFloatPoint(51, 50), WebFloatPoint(51, 50),
      WebPointerProperties::Button::kLeft, 0, WebInputEvent::kNoModifiers,
      CurrentTimeTicks());
  mouse_press_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_press_event);
  ASSERT_FALSE(chrome_client_->ReceivedRequestForUnbufferedInput());

  canvas.SetNeedsUnbufferedInputEvents(true);

  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_press_event);
  ASSERT_TRUE(chrome_client_->ReceivedRequestForUnbufferedInput());

  canvas.SetNeedsUnbufferedInputEvents(false);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_press_event);
  ASSERT_FALSE(chrome_client_->ReceivedRequestForUnbufferedInput());
}

class NavigationCapturingFrameClient : public EmptyLocalFrameClient {
 public:
  NavigationCapturingFrameClient() = default;

  bool NavigateBackForward(int offset) const override {
    offset_ = offset;
    return true;
  }

  int Offset() const { return offset_; }

 private:
  mutable int offset_ = 0;
};

class EventHandlerNavigationTest : public EventHandlerTest {
 public:
  EventHandlerNavigationTest() = default;

  void SetUp() override {
    frame_client_ = MakeGarbageCollected<NavigationCapturingFrameClient>();
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    SetupPageWithClients(&clients, frame_client_);
  }

  int Offset() { return frame_client_->Offset(); }

 private:
  Persistent<NavigationCapturingFrameClient> frame_client_;
};

TEST_F(EventHandlerNavigationTest, MouseButtonsNavigate) {
  SetHtmlInnerHTML("<div>");

  EXPECT_EQ(0, Offset());

  WebMouseEvent mouse_back_event(
      WebInputEvent::kMouseUp, WebFloatPoint(51, 50), WebFloatPoint(51, 50),
      WebPointerProperties::Button::kBack, 0, WebInputEvent::kNoModifiers,
      CurrentTimeTicks());
  mouse_back_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_back_event);

  EXPECT_EQ(-1, Offset());

  WebMouseEvent mouse_forward_event(
      WebInputEvent::kMouseUp, WebFloatPoint(51, 50), WebFloatPoint(51, 50),
      WebPointerProperties::Button::kForward, 0, WebInputEvent::kNoModifiers,
      CurrentTimeTicks());
  mouse_forward_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_forward_event);

  EXPECT_EQ(1, Offset());
}

TEST_F(EventHandlerNavigationTest, MouseButtonsDontNavigate) {
  SetHtmlInnerHTML("<div>");
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->SetInnerHTMLFromString(
      "document.addEventListener('mouseup', event => "
      "event.preventDefault());");
  GetDocument().body()->AppendChild(script);

  EXPECT_EQ(0, Offset());

  WebMouseEvent mouse_back_event(
      WebInputEvent::kMouseUp, WebFloatPoint(51, 50), WebFloatPoint(51, 50),
      WebPointerProperties::Button::kBack, 0, WebInputEvent::kNoModifiers,
      CurrentTimeTicks());
  mouse_back_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_back_event);

  EXPECT_EQ(0, Offset());

  WebMouseEvent mouse_forward_event(
      WebInputEvent::kMouseUp, WebFloatPoint(51, 50), WebFloatPoint(51, 50),
      WebPointerProperties::Button::kForward, 0, WebInputEvent::kNoModifiers,
      CurrentTimeTicks());
  mouse_forward_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_forward_event);

  EXPECT_EQ(0, Offset());
}

class InjectedGestureCapturingChromeClient : public EmptyChromeClient {
 public:
  InjectedGestureCapturingChromeClient() = default;

  struct InjectedGestureData {
    WebFloatSize delta;
    ScrollGranularity granularity;
    CompositorElementId scrollable_area_element_id;
    WebInputEvent::Type type;
  };

  void InjectGestureScrollEvent(LocalFrame& local_frame,
                                WebGestureDevice device,
                                const WebFloatSize& delta,
                                ScrollGranularity granularity,
                                CompositorElementId scrollable_area_element_id,
                                WebInputEvent::Type type) override {
    InjectedGestureData data{delta, granularity, scrollable_area_element_id,
                             type};
    injected_data_.push_back(data);
  }

  const std::vector<InjectedGestureData>& GetInjectedData() const {
    return injected_data_;
  }

 private:
  std::vector<InjectedGestureData> injected_data_;
};

class EventHandlerScrollbarGestureInjectionTest : public EventHandlerTest {
 public:
  EventHandlerScrollbarGestureInjectionTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kScrollbarInjectScrollGestures);
    chrome_client_ =
        MakeGarbageCollected<InjectedGestureCapturingChromeClient>();
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();
    SetupPageWithClients(&clients);
  }

  const std::vector<InjectedGestureCapturingChromeClient::InjectedGestureData>&
  GetInjectedData() {
    return chrome_client_->GetInjectedData();
  }

 private:
  Persistent<InjectedGestureCapturingChromeClient> chrome_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(EventHandlerScrollbarGestureInjectionTest,
       MouseUpOffScrollbarGeneratesScrollEnd) {
  SetHtmlInnerHTML(
      "<div style=\"height:1000px\">"
      "Tall text to create viewport scrollbar</div>");
  EXPECT_EQ(GetInjectedData().size(), 0ULL);

  // PageTestBase sizes the page to 800x600. Click on the scrollbar
  // track, move off, then release the mouse and verify that GestureScrollEnd
  // was queued up.

  // If the scrollbar theme does not allow hit testing, we should not get
  // any injected gesture events. Mobile overlay scrollbar theme does not
  // allow hit testing.
  bool scrollbar_theme_allows_hit_test =
      GetPage().GetScrollbarTheme().AllowsHitTest();

  const WebFloatPoint scrollbar_forward_track(795, 560);
  WebMouseEvent mouse_down(WebInputEvent::kMouseDown, scrollbar_forward_track,
                           scrollbar_forward_track,
                           WebPointerProperties::Button::kLeft, 0,
                           WebInputEvent::kNoModifiers, CurrentTimeTicks());
  mouse_down.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(mouse_down);

  // Mouse down on the scrollbar track should have generated GSB/GSU.
  if (scrollbar_theme_allows_hit_test) {
    EXPECT_EQ(GetInjectedData().size(), 2ULL);
    EXPECT_EQ(GetInjectedData()[0].type, WebInputEvent::kGestureScrollBegin);
    EXPECT_EQ(GetInjectedData()[1].type, WebInputEvent::kGestureScrollUpdate);
  } else {
    EXPECT_EQ(GetInjectedData().size(), 0ULL);
  }

  const WebFloatPoint middle_of_page(100, 100);
  WebMouseEvent mouse_move(WebInputEvent::kMouseMove, middle_of_page,
                           middle_of_page, WebPointerProperties::Button::kLeft,
                           0, WebInputEvent::kNoModifiers, CurrentTimeTicks());
  mouse_move.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  // Mouse move should not have generated any gestures.
  if (scrollbar_theme_allows_hit_test) {
    EXPECT_EQ(GetInjectedData().size(), 2ULL);
  } else {
    EXPECT_EQ(GetInjectedData().size(), 0ULL);
  }

  WebMouseEvent mouse_up(WebInputEvent::kMouseUp, middle_of_page,
                         middle_of_page, WebPointerProperties::Button::kLeft, 0,
                         WebInputEvent::kNoModifiers, CurrentTimeTicks());
  mouse_up.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(mouse_up);

  // Mouse up must generate GestureScrollEnd.
  if (scrollbar_theme_allows_hit_test) {
    EXPECT_EQ(GetInjectedData().size(), 3ULL);
    EXPECT_EQ(GetInjectedData()[2].type, WebInputEvent::kGestureScrollEnd);
  } else {
    EXPECT_EQ(GetInjectedData().size(), 0ULL);
  }
}

TEST_F(EventHandlerScrollbarGestureInjectionTest, MouseUpOnlyOnScrollbar) {
  SetHtmlInnerHTML(
      "<div style=\"height:1000px\">"
      "Tall text to create viewport scrollbar</div>");
  EXPECT_EQ(GetInjectedData().size(), 0ULL);

  // Mouse down on the page, the move the mouse to the scrollbar and release.
  // Validate that we don't inject a ScrollEnd (since no ScrollBegin was
  // injected).

  const WebFloatPoint middle_of_page(100, 100);
  WebMouseEvent mouse_down(WebInputEvent::kMouseDown, middle_of_page,
                           middle_of_page, WebPointerProperties::Button::kLeft,
                           0, WebInputEvent::kNoModifiers, CurrentTimeTicks());
  mouse_down.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(mouse_down);

  // Mouse down on the page should not generate scroll gestures.
  EXPECT_EQ(GetInjectedData().size(), 0ULL);

  const WebFloatPoint scrollbar_forward_track(795, 560);
  WebMouseEvent mouse_move(WebInputEvent::kMouseMove, scrollbar_forward_track,
                           scrollbar_forward_track,
                           WebPointerProperties::Button::kLeft, 0,
                           WebInputEvent::kNoModifiers, CurrentTimeTicks());
  mouse_move.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  // Mouse move should not have generated any gestures.
  EXPECT_EQ(GetInjectedData().size(), 0ULL);

  WebMouseEvent mouse_up(WebInputEvent::kMouseUp, scrollbar_forward_track,
                         scrollbar_forward_track,
                         WebPointerProperties::Button::kLeft, 0,
                         WebInputEvent::kNoModifiers, CurrentTimeTicks());
  mouse_up.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(mouse_up);

  // Mouse up should not have generated any gestures.
  EXPECT_EQ(GetInjectedData().size(), 0ULL);
}

TEST_F(EventHandlerScrollbarGestureInjectionTest, RightClickNoGestures) {
  SetHtmlInnerHTML(
      "<div style=\"height:1000px\">"
      "Tall text to create viewport scrollbar</div>");
  EXPECT_EQ(GetInjectedData().size(), 0ULL);

  // PageTestBase sizes the page to 800x600. Right click on the scrollbar
  // track, and release the mouse and verify that no gesture events are
  // queued up (right click doesn't scroll scrollbars).

  const WebFloatPoint scrollbar_forward_track(795, 560);
  WebMouseEvent mouse_down(WebInputEvent::kMouseDown, scrollbar_forward_track,
                           scrollbar_forward_track,
                           WebPointerProperties::Button::kRight, 0,
                           WebInputEvent::kNoModifiers, CurrentTimeTicks());
  mouse_down.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(mouse_down);

  EXPECT_EQ(GetInjectedData().size(), 0ULL);

  WebMouseEvent mouse_up(WebInputEvent::kMouseUp, scrollbar_forward_track,
                         scrollbar_forward_track,
                         WebPointerProperties::Button::kRight, 0,
                         WebInputEvent::kNoModifiers, CurrentTimeTicks());
  mouse_up.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(mouse_up);

  EXPECT_EQ(GetInjectedData().size(), 0ULL);
}

// Test that leaving a window leaves mouse position unknown.
TEST_F(EventHandlerTest, MouseLeaveResetsUnknownState) {
  SetHtmlInnerHTML("<div></div>");
  WebMouseEvent mouse_down_event(WebMouseEvent::kMouseDown,
                                 WebFloatPoint(262, 29), WebFloatPoint(329, 67),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().IsMousePositionUnknown());

  WebMouseEvent mouse_leave_event(
      WebMouseEvent::kMouseLeave, WebFloatPoint(262, 29),
      WebFloatPoint(329, 67), WebPointerProperties::Button::kNoButton, 1,
      WebInputEvent::Modifiers::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  mouse_leave_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseLeaveEvent(
      mouse_leave_event);
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().IsMousePositionUnknown());
}

// Test that leaving an iframe sets the mouse position to unknown on that
// iframe.
TEST_F(EventHandlerSimTest, MouseLeaveIFrameResets) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));

  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    div {
      width: 200px;
      height: 200px;
    }
    iframe {
      width: 200px;
      height: 200px;
    }
    </style>
    <div></div>
    <iframe id='frame' src='frame.html'></iframe>
  )HTML");

  frame_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();
  WebMouseEvent mouse_move_inside_event(
      WebMouseEvent::kMouseMove, WebFloatPoint(100, 229),
      WebFloatPoint(100, 229), WebPointerProperties::Button::kNoButton, 0,
      WebInputEvent::Modifiers::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  mouse_move_inside_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_inside_event, Vector<WebMouseEvent>(),
      Vector<WebMouseEvent>());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().IsMousePositionUnknown());
  auto* child_frame =
      ToHTMLIFrameElement(GetDocument().getElementById("frame"));
  child_frame->contentDocument()->UpdateStyleAndLayout();
  EXPECT_TRUE(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_FALSE(To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild())
                   ->GetEventHandler()
                   .IsMousePositionUnknown());

  WebMouseEvent mouse_move_outside_event(
      WebMouseEvent::kMouseMove, WebFloatPoint(300, 29), WebFloatPoint(300, 29),
      WebPointerProperties::Button::kNoButton, 0,
      WebInputEvent::Modifiers::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  mouse_move_outside_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_outside_event, Vector<WebMouseEvent>(),
      Vector<WebMouseEvent>());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().IsMousePositionUnknown());
  EXPECT_TRUE(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild())
                  ->GetEventHandler()
                  .IsMousePositionUnknown());
}

// Test that mouse down and move a small distance on a draggable element will
// not change cursor style.
TEST_F(EventHandlerSimTest, CursorStyleBeforeStartDragging) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    div {
      width: 300px;
      height: 100px;
      cursor: help;
    }
    </style>
    <div draggable='true'>foo</div>
  )HTML");
  Compositor().BeginFrame();

  WebMouseEvent mouse_down_event(WebMouseEvent::kMouseDown,
                                 WebFloatPoint(150, 50), WebFloatPoint(150, 50),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent mouse_move_event(WebMouseEvent::kMouseMove,
                                 WebFloatPoint(151, 50), WebFloatPoint(151, 50),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  EXPECT_EQ(Cursor::Type::kHelp, GetDocument()
                                     .GetFrame()
                                     ->GetChromeClient()
                                     .LastSetCursorForTesting()
                                     .GetType());
}

// Ensure that tap on element in iframe should apply active state.
TEST_F(EventHandlerSimTest, TapActiveInFrame) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));

  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    iframe {
      width: 200px;
      height: 200px;
    }
    </style>
    <iframe id='iframe' src='iframe.html'>
    </iframe>
  )HTML");

  frame_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    div {
      width: 100px;
      height: 100px;
    }
    </style>
    <div></div>
  )HTML");
  Compositor().BeginFrame();

  auto* iframe_element =
      ToHTMLIFrameElement(GetDocument().getElementById("iframe"));
  Document* iframe_doc = iframe_element->contentDocument();

  TapDownEventBuilder tap_down(FloatPoint(10, 10));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(tap_down);

  ShowPressEventBuilder show_press(FloatPoint(10, 10));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(show_press);

  // TapDown and ShowPress active the iframe.
  EXPECT_TRUE(GetDocument().GetActiveElement());
  EXPECT_TRUE(iframe_doc->GetActiveElement());

  TapEventBuilder tap(FloatPoint(10, 10), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(tap);

  // Should still active.
  EXPECT_TRUE(GetDocument().GetActiveElement());
  EXPECT_TRUE(iframe_doc->GetActiveElement());

  // The active will cancel after 15ms.
  test::RunDelayedTasks(TimeDelta::FromSecondsD(0.2));
  EXPECT_FALSE(GetDocument().GetActiveElement());
  EXPECT_FALSE(iframe_doc->GetActiveElement());
}

// Test that the hover is updated at the next begin frame after the compositor
// scroll ends.
TEST_F(EventHandlerSimTest, TestUpdateHoverAfterCompositorScrollAtBeginFrame) {
  ScopedUpdateHoverFromScrollAtBeginFrameForTest scoped_feature(true);
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
      }
      div {
        height: 300px;
        width: 100%;
      }
    </style>
    <body>
    <div class="hoverme" id="line1">hover over me</div>
    <div class="hoverme" id="line2">hover over me</div>
    <div class="hoverme" id="line3">hover over me</div>
    <div class="hoverme" id="line4">hover over me</div>
    <div class="hoverme" id="line5">hover over me</div>
    </body>
    <script>
      let array = document.getElementsByClassName('hoverme');
      for (let element of array) {
        element.addEventListener('mouseover', function (e) {
          this.innerHTML = "currently hovered";
        });
        element.addEventListener('mouseout', function (e) {
          this.innerHTML = "was hovered";
        });
      }
    </script>
  )HTML");
  Compositor().BeginFrame();

  // Set mouse position and active web view.
  WebMouseEvent mouse_down_event(WebMouseEvent::kMouseDown, WebFloatPoint(1, 1),
                                 WebFloatPoint(1, 1),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent mouse_up_event(
      WebInputEvent::kMouseUp, WebFloatPoint(1, 1), WebFloatPoint(1, 1),
      WebPointerProperties::Button::kLeft, 1, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  mouse_up_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_up_event);

  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);

  WebElement element1 = GetDocument().getElementById("line1");
  WebElement element2 = GetDocument().getElementById("line2");
  WebElement element3 = GetDocument().getElementById("line3");
  EXPECT_EQ("currently hovered", element1.InnerHTML().Utf8());
  EXPECT_EQ("hover over me", element2.InnerHTML().Utf8());
  EXPECT_EQ("hover over me", element3.InnerHTML().Utf8());

  // Do a compositor scroll and set |hover_needs_update_at_scroll_end| to be
  // true in WebViewImpl.
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewport()->DidScroll(FloatPoint(0, 500));
  WebView().MainFrameWidget()->ApplyViewportChanges(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 1.0f, false, 0,
       cc::BrowserControlsState::kBoth, true});
  ASSERT_EQ(500, frame_view->LayoutViewport()->GetScrollOffset().Height());
  EXPECT_EQ("currently hovered", element1.InnerHTML().Utf8());
  EXPECT_EQ("hover over me", element2.InnerHTML().Utf8());
  EXPECT_EQ("hover over me", element3.InnerHTML().Utf8());

  // The fake mouse move event is dispatched at the begin frame to update hover.
  Compositor().BeginFrame();
  EXPECT_EQ("was hovered", element1.InnerHTML().Utf8());
  EXPECT_EQ("currently hovered", element2.InnerHTML().Utf8());
  EXPECT_EQ("hover over me", element3.InnerHTML().Utf8());
}

// Test that the hover is updated at the next begin frame after the main thread
// scroll ends.
TEST_F(EventHandlerSimTest, TestUpdateHoverAfterMainThreadScrollAtBeginFrame) {
  ScopedUpdateHoverFromScrollAtBeginFrameForTest scoped_feature(true);
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
      }
      div {
        height: 300px;
        width: 100%;
      }
    </style>
    <body>
    <div class="hoverme" id="line1">hover over me</div>
    <div class="hoverme" id="line2">hover over me</div>
    <div class="hoverme" id="line3">hover over me</div>
    <div class="hoverme" id="line4">hover over me</div>
    <div class="hoverme" id="line5">hover over me</div>
    </body>
    <script>
      let array = document.getElementsByClassName('hoverme');
      for (let element of array) {
        element.addEventListener('mouseover', function (e) {
          this.innerHTML = "currently hovered";
        });
        element.addEventListener('mouseout', function (e) {
          this.innerHTML = "was hovered";
        });
      }
    </script>
  )HTML");
  Compositor().BeginFrame();

  // Set mouse position and active web view.
  WebMouseEvent mouse_down_event(WebMouseEvent::kMouseDown, WebFloatPoint(1, 1),
                                 WebFloatPoint(1, 1),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent mouse_up_event(
      WebInputEvent::kMouseUp, WebFloatPoint(1, 1), WebFloatPoint(1, 1),
      WebPointerProperties::Button::kLeft, 1, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  mouse_up_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_up_event);

  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);

  WebElement element1 = GetDocument().getElementById("line1");
  WebElement element2 = GetDocument().getElementById("line2");
  WebElement element3 = GetDocument().getElementById("line3");
  EXPECT_EQ("currently hovered", element1.InnerHTML().Utf8());
  EXPECT_EQ("hover over me", element2.InnerHTML().Utf8());
  EXPECT_EQ("hover over me", element3.InnerHTML().Utf8());

  // Send scroll gesture events which will set
  // |hover_needs_update_at_scroll_end_| to be true in ScrollManager.
  WebGestureEvent scroll_begin_event(
      WebInputEvent::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchpad);
  WebGestureEvent scroll_update_event(
      WebInputEvent::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchpad);
  scroll_update_event.data.scroll_update.delta_y = -500;
  WebGestureEvent scroll_end_event(WebInputEvent::kGestureScrollEnd,
                                   WebInputEvent::kNoModifiers,
                                   WebInputEvent::GetStaticTimeStampForTests(),
                                   blink::WebGestureDevice::kTouchpad);
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(scroll_begin_event));
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(scroll_update_event));
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(scroll_end_event));
  LocalFrameView* frame_view = GetDocument().View();
  ASSERT_EQ(500, frame_view->LayoutViewport()->GetScrollOffset().Height());
  EXPECT_EQ("currently hovered", element1.InnerHTML().Utf8());
  EXPECT_EQ("hover over me", element2.InnerHTML().Utf8());
  EXPECT_EQ("hover over me", element3.InnerHTML().Utf8());

  // The fake mouse move event is dispatched at the begin frame to update hover.
  Compositor().BeginFrame();
  EXPECT_EQ("was hovered", element1.InnerHTML().Utf8());
  EXPECT_EQ("currently hovered", element2.InnerHTML().Utf8());
  EXPECT_EQ("hover over me", element3.InnerHTML().Utf8());
}

// Test that the hover is updated at the next begin frame after the smooth JS
// scroll ends.
TEST_F(EventHandlerSimTest, TestUpdateHoverAfterJSScrollAtBeginFrame) {
  ScopedUpdateHoverFromScrollAtBeginFrameForTest scoped_feature(true);
  WebView().MainFrameWidget()->Resize(WebSize(800, 500));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
        height: 500vh;
      }
      div {
        height: 500px;
        width: 100%;
      }
    </style>
    <body>
    <div class="hoverme" id="hoverarea">hover over me</div>
    </body>
  )HTML");
  Compositor().BeginFrame();

  // Set mouse position and active web view.
  WebMouseEvent mouse_move_event(
      WebMouseEvent::kMouseMove, WebFloatPoint(100, 100),
      WebFloatPoint(100, 100), WebPointerProperties::Button::kNoButton, 0,
      WebInputEvent::Modifiers::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);

  Element* element = GetDocument().getElementById("hoverarea");
  EXPECT_TRUE(element->IsHovered());

  // Find the scrollable area and set scroll offset.
  ScrollableArea* scrollable_area =
      GetDocument().GetLayoutView()->GetScrollableArea();
  bool finished = false;
  scrollable_area->SetScrollOffset(
      ScrollOffset(0, 1000), kProgrammaticScroll, kScrollBehaviorSmooth,
      ScrollableArea::ScrollCallback(
          base::BindOnce([](bool* finished) { *finished = true; }, &finished)));
  Compositor().BeginFrame();
  LocalFrameView* frame_view = GetDocument().View();
  ASSERT_EQ(0, frame_view->LayoutViewport()->GetScrollOffset().Height());
  ASSERT_FALSE(finished);
  // Scrolling is in progress but the hover is not updated yet.
  Compositor().BeginFrame();
  // Start scroll animation, but it is not finished.
  Compositor().BeginFrame();
  ASSERT_GT(frame_view->LayoutViewport()->GetScrollOffset().Height(), 0);
  ASSERT_FALSE(finished);

  // Mark hover state dirty but the hover state does not change after the
  // animation finishes.
  Compositor().BeginFrame(1);
  ASSERT_EQ(1000, frame_view->LayoutViewport()->GetScrollOffset().Height());
  ASSERT_TRUE(finished);
  EXPECT_TRUE(element->IsHovered());

  // Hover state is updated after the begin frame.
  Compositor().BeginFrame();
  EXPECT_FALSE(element->IsHovered());
}

TEST_F(EventHandlerSimTest, LargeCustomCursorIntersectsViewport) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  SimSubresourceRequest cursor_request("https://example.com/100x100.png",
                                       "image/png");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
        <!DOCTYPE html>
        <style>
        div {
          width: 300px;
          height: 100px;
          cursor: url('100x100.png') 100 100, auto;
        }
        </style>
        <div>foo</div>
      )HTML");

  GetDocument().UpdateStyleAndLayoutTree();

  scoped_refptr<SharedBuffer> img =
      test::ReadFromFile(test::CoreTestDataPath("notifications/100x100.png"));
  cursor_request.Complete(img->CopyAs<Vector<char>>());

  Compositor().BeginFrame();

  // Move the cursor so no part of it intersects the viewport.
  {
    WebMouseEvent mouse_move_event(
        WebMouseEvent::kMouseMove, WebFloatPoint(101, 101),
        WebFloatPoint(101, 101), WebPointerProperties::Button::kNoButton, 0, 0,
        WebInputEvent::GetStaticTimeStampForTests());
    mouse_move_event.SetFrameScale(1);
    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
        mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

    const Cursor& cursor =
        GetDocument().GetFrame()->GetChromeClient().LastSetCursorForTesting();
    EXPECT_EQ(Cursor::Type::kCustom, cursor.GetType());
  }

  // Now, move the cursor so that it intersects the visual viewport. The cursor
  // should be removed.
  {
    WebMouseEvent mouse_move_event(
        WebMouseEvent::kMouseMove, WebFloatPoint(99, 99), WebFloatPoint(99, 99),
        WebPointerProperties::Button::kNoButton, 0, 0,
        WebInputEvent::GetStaticTimeStampForTests());
    mouse_move_event.SetFrameScale(1);
    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
        mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

    const Cursor& cursor =
        GetDocument().GetFrame()->GetChromeClient().LastSetCursorForTesting();
    EXPECT_EQ(Cursor::Type::kPointer, cursor.GetType());
  }
}

TEST_F(EventHandlerSimTest, SmallCustomCursorIntersectsViewport) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  SimSubresourceRequest cursor_request("https://example.com/48x48.png",
                                       "image/png");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
        <!DOCTYPE html>
        <style>
        div {
          width: 300px;
          height: 100px;
          cursor: -webkit-image-set(url('48x48.png') 2x) 24 24, auto;
        }
        </style>
        <div>foo</div>
      )HTML");

  GetDocument().UpdateStyleAndLayoutTree();

  scoped_refptr<SharedBuffer> img =
      test::ReadFromFile(test::CoreTestDataPath("notifications/48x48.png"));
  cursor_request.Complete(img->CopyAs<Vector<char>>());

  Compositor().BeginFrame();

  // Move the cursor so no part of it intersects the viewport.
  {
    WebMouseEvent mouse_move_event(
        WebMouseEvent::kMouseMove, WebFloatPoint(25, 25), WebFloatPoint(25, 25),
        WebPointerProperties::Button::kNoButton, 0, 0,
        WebInputEvent::GetStaticTimeStampForTests());
    mouse_move_event.SetFrameScale(1);
    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
        mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

    const Cursor& cursor =
        GetDocument().GetFrame()->GetChromeClient().LastSetCursorForTesting();
    EXPECT_EQ(Cursor::Type::kCustom, cursor.GetType());
  }

  // Now, move the cursor so that it intersects the visual viewport. The cursor
  // should not be removed because it is below
  // kMaximumCursorSizeWithoutFallback.
  {
    WebMouseEvent mouse_move_event(
        WebMouseEvent::kMouseMove, WebFloatPoint(23, 23), WebFloatPoint(23, 23),
        WebPointerProperties::Button::kNoButton, 0, 0,
        WebInputEvent::GetStaticTimeStampForTests());
    mouse_move_event.SetFrameScale(1);
    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
        mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

    const Cursor& cursor =
        GetDocument().GetFrame()->GetChromeClient().LastSetCursorForTesting();
    EXPECT_EQ(Cursor::Type::kCustom, cursor.GetType());
  }
}

TEST_F(EventHandlerSimTest, NotExposeKeyboardEvent) {
  GetDocument().GetSettings()->SetDontSendKeyEventsToJavascript(true);
  GetDocument().GetSettings()->SetScrollAnimatorEnabled(false);
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      height: 10000px;
    }
    </style>
    Last event: <br>
    <p id='log'>no event</p>
    <input id="input1" type="text">

    <script>
      document.addEventListener('keydown', (e) => {
        let log = document.getElementById('log');
        log.innerText = 'keydown cancelable=' + e.cancelable;
      });
      document.addEventListener('keyup', (e) => {
        let log = document.getElementById('log');
        log.innerText = 'keyup cancelable=' + e.cancelable;
      });
    </script>
  )HTML");
  Compositor().BeginFrame();

  WebElement element = GetDocument().getElementById("log");
  WebKeyboardEvent e{WebInputEvent::kRawKeyDown, WebInputEvent::kNoModifiers,
                     WebInputEvent::GetStaticTimeStampForTests()};
  e.windows_key_code = VKEY_DOWN;
  // TODO(crbug.com/949766) Should cleanup these magic number.
  e.dom_key = 0x00200309;
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("no event", element.InnerHTML().Utf8());

  e.SetType(WebInputEvent::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("no event", element.InnerHTML().Utf8());

  e.SetType(WebInputEvent::kKeyDown);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("no event", element.InnerHTML().Utf8());

  e.SetType(WebInputEvent::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("no event", element.InnerHTML().Utf8());

  // Key send to js but not cancellable.
  e.dom_key = 0x00400031;
  e.SetType(WebInputEvent::kRawKeyDown);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("keydown cancelable=false", element.InnerHTML().Utf8());

  e.SetType(WebInputEvent::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("keyup cancelable=false", element.InnerHTML().Utf8());

  // Key send to js and cancellable in editor.
  WebElement input = GetDocument().getElementById("input1");
  GetDocument().SetFocusedElement(
      input.Unwrap<Element>(),
      FocusParams(SelectionBehaviorOnFocus::kNone, kWebFocusTypeNone, nullptr));

  e.SetType(WebInputEvent::kRawKeyDown);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("keydown cancelable=true", element.InnerHTML().Utf8());

  // Arrow key caused scroll down in post event dispatch process. Ensure page
  // scrolled.
  ScrollableArea* scrollable_area = GetDocument().View()->LayoutViewport();
  EXPECT_GT(scrollable_area->ScrollOffsetInt().Height(), 0);
}

TEST_F(EventHandlerSimTest, DoNotScrollWithTouchpadIfOverflowIsHidden) {
  WebView().MainFrameWidget()->Resize(WebSize(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #outer {
        width: 100vw;
        height: 100vh;
        overflow-x: hidden;
        overflow-y: scroll;
    }
    #inner {
        width: 300vw;
        height: 300vh;
    }
    </style>
    <body>
      <div id='outer'>
        <div id='inner'>
      </div>
    </body>
  )HTML");
  Compositor().BeginFrame();

  WebGestureEvent scroll_begin_event(
      WebInputEvent::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchpad);
  scroll_begin_event.SetPositionInWidget(WebFloatPoint(10, 10));
  scroll_begin_event.SetPositionInScreen(WebFloatPoint(10, 10));
  scroll_begin_event.SetFrameScale(1);

  WebGestureEvent scroll_update_event(
      WebInputEvent::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchpad);
  scroll_update_event.data.scroll_update.delta_x = -100;
  scroll_update_event.data.scroll_update.delta_y = -100;
  scroll_update_event.SetPositionInWidget(WebFloatPoint(10, 10));
  scroll_update_event.SetPositionInScreen(WebFloatPoint(10, 10));
  scroll_update_event.SetFrameScale(1);

  WebGestureEvent scroll_end_event(WebInputEvent::kGestureScrollEnd,
                                   WebInputEvent::kNoModifiers,
                                   WebInputEvent::GetStaticTimeStampForTests(),
                                   blink::WebGestureDevice::kTouchpad);
  scroll_end_event.SetPositionInWidget(WebFloatPoint(10, 10));
  scroll_end_event.SetPositionInScreen(WebFloatPoint(10, 10));

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(scroll_begin_event));
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(scroll_update_event));
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(scroll_end_event));

  EXPECT_EQ(0, GetDocument().getElementById("outer")->scrollLeft());
}

TEST_F(EventHandlerSimTest, ElementTargetedGestureScroll) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #scroller {
        overflow-y:scroll;
        height:200px;
      }
      #talldiv {
        height:1000px;
      }
    </style>
    <div id="talldiv">Tall text to create viewport scrollbar</div>
    <div id="scroller">
      <div style="height:2000px">To create subscroller scrollbar</div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Element* const scroller = GetDocument().getElementById("scroller");
  constexpr float delta_y = 100;
  // Send GSB/GSU at 0,0 to target the viewport first, then verify that
  // the viewport scrolled accordingly.
  WebGestureEvent gesture_scroll_begin{
      WebInputEvent::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests()};
  gesture_scroll_begin.SetFrameScale(1);
  gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0;
  gesture_scroll_begin.data.scroll_begin.delta_y_hint = -delta_y;
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_scroll_begin);

  WebGestureEvent gesture_scroll_update{
      WebInputEvent::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests()};
  gesture_scroll_update.SetFrameScale(1);
  gesture_scroll_update.data.scroll_update.delta_x = 0;
  gesture_scroll_update.data.scroll_update.delta_y = -delta_y;

  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_scroll_update);

  WebGestureEvent gesture_scroll_end{
      WebInputEvent::kGestureScrollEnd, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests()};
  gesture_scroll_end.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_scroll_end);

  LocalFrameView* frame_view = GetDocument().View();
  ASSERT_EQ(frame_view->LayoutViewport()->GetScrollOffset().Height(), delta_y);

  // Switch to the element_id-based targeting for GSB, then resend GSU
  // and validate that the subscroller scrolled (and that the viewport
  // did not).
  ScrollableArea* scrollable_area =
      scroller->GetLayoutBox()->GetScrollableArea();
  gesture_scroll_begin.data.scroll_begin.scrollable_area_element_id =
      scrollable_area->GetCompositorElementId().GetInternalValue();

  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_scroll_begin);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_scroll_update);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_scroll_end);

  ASSERT_EQ(scrollable_area->ScrollOffsetInt().Height(), delta_y);
  ASSERT_EQ(frame_view->LayoutViewport()->GetScrollOffset().Height(), delta_y);

  // Remove the scroller, update layout, and ensure the same gestures
  // don't crash or scroll the layout viewport.
  scroller->remove();
  GetDocument().UpdateStyleAndLayout();
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_scroll_begin);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_scroll_update);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_scroll_end);

  ASSERT_EQ(frame_view->LayoutViewport()->GetScrollOffset().Height(), delta_y);
}

TEST_F(EventHandlerSimTest, ElementTargetedGestureScrollIFrame) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request_outer("https://example.com/test-outer.html", "text/html");
  SimRequest request_inner("https://example.com/test-inner.html", "text/html");
  LoadURL("https://example.com/test-outer.html");
  request_outer.Complete(R"HTML(
    <!DOCTYPE html>
    <iframe id="iframe" src="test-inner.html"></iframe>
    <div style="height:1000px"></div>
    )HTML");

  request_inner.Complete(R"HTML(
    <!DOCTYPE html>
    <div style="height:1000px"></div>
  )HTML");
  Compositor().BeginFrame();

  HTMLFrameElementBase* const iframe =
      ToHTMLFrameElementBase(GetDocument().getElementById("iframe"));
  FrameView* child_frame_view =
      iframe->GetLayoutEmbeddedContent()->ChildFrameView();
  auto* local_child_frame_view = DynamicTo<LocalFrameView>(child_frame_view);
  ScrollableArea* scrollable_area = local_child_frame_view->GetScrollableArea();

  // Target the iframe scrollable area and make sure it scrolls when targeted
  // with gestures.
  constexpr float delta_y = 100;
  WebGestureEvent gesture_scroll_begin{
      WebInputEvent::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests()};
  gesture_scroll_begin.SetFrameScale(1);
  gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0;
  gesture_scroll_begin.data.scroll_begin.delta_y_hint = -delta_y;
  gesture_scroll_begin.data.scroll_begin.scrollable_area_element_id =
      scrollable_area->GetCompositorElementId().GetInternalValue();
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_scroll_begin);

  WebGestureEvent gesture_scroll_update{
      WebInputEvent::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests()};
  gesture_scroll_update.SetFrameScale(1);
  gesture_scroll_update.data.scroll_update.delta_x = 0;
  gesture_scroll_update.data.scroll_update.delta_y = -delta_y;

  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_scroll_update);

  WebGestureEvent gesture_scroll_end{
      WebInputEvent::kGestureScrollEnd, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests()};
  gesture_scroll_end.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_scroll_end);

  LocalFrameView* frame_view = GetDocument().View();
  ASSERT_EQ(frame_view->LayoutViewport()->GetScrollOffset().Height(), 0);
  ASSERT_EQ(scrollable_area->ScrollOffsetInt().Height(), delta_y);
}

TEST_F(EventHandlerSimTest, ElementTargetedGestureScrollViewport) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  // Set a page scale factor so that the VisualViewport will also scroll.
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div style="height:1000px">Tall text to create viewport scrollbar</div>
  )HTML");
  WebView().SetPageScaleFactor(2);
  Compositor().BeginFrame();

  // Target the visual viewport (which is a ScrollableArea), and validate
  // that the layout viewport scrolled.
  constexpr float delta_y = 700;
  const VisualViewport& visual_viewport =
      GetDocument().GetPage()->GetVisualViewport();

  WebGestureEvent gesture_scroll_begin{
      WebInputEvent::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests()};
  gesture_scroll_begin.SetFrameScale(1);
  gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0;
  gesture_scroll_begin.data.scroll_begin.delta_y_hint = -delta_y;
  gesture_scroll_begin.data.scroll_begin.scrollable_area_element_id =
      visual_viewport.GetCompositorElementId().GetInternalValue();

  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_scroll_begin);

  WebGestureEvent gesture_scroll_update{
      WebInputEvent::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests()};
  gesture_scroll_update.SetFrameScale(1);
  gesture_scroll_update.data.scroll_update.delta_x = 0;
  gesture_scroll_update.data.scroll_update.delta_y = -delta_y;

  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_scroll_update);

  WebGestureEvent gesture_scroll_end{
      WebInputEvent::kGestureScrollEnd, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests()};
  gesture_scroll_end.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_scroll_end);

  LocalFrameView* frame_view = GetDocument().View();
  ASSERT_EQ(frame_view->LayoutViewport()->GetScrollOffset().Height(), 400);
  ASSERT_EQ(visual_viewport.GetScrollOffset().Height(), 300);
}

TEST_F(EventHandlerSimTest, SelecteTransformedTextWhenCapturing) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
      <div id='target' style = "width:250px; transform: rotate(180deg)">
      Some text to select
      </div>
  )HTML");
  Compositor().BeginFrame();

  WebMouseEvent mouse_down_event(WebInputEvent::kMouseDown,
                                 WebFloatPoint(100, 20), WebFloatPoint(0, 0),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  ASSERT_TRUE(GetDocument()
                  .GetFrame()
                  ->GetEventHandler()
                  .GetSelectionController()
                  .MouseDownMayStartSelect());

  Element* target = GetDocument().getElementById("target");
  GetDocument().GetFrame()->GetEventHandler().SetPointerCapture(
      PointerEventFactory::kMouseId, target);

  WebMouseEvent mouse_move_event(WebInputEvent::kMouseMove,
                                 WebFloatPoint(258, 20), WebFloatPoint(0, 0),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  WebMouseEvent mouse_up_event(
      WebMouseEvent::kMouseUp, WebFloatPoint(258, 20), WebFloatPoint(0, 0),
      WebPointerProperties::Button::kLeft, 1, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  mouse_up_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_up_event);

  ASSERT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .GetSelectionController()
                   .MouseDownMayStartSelect());

  ASSERT_TRUE(GetDocument().GetSelection());
  EXPECT_EQ("Some text to select", GetDocument().GetSelection()->toString());
}

}  // namespace blink
