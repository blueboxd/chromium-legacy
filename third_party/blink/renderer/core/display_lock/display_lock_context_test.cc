// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/find_in_page.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {
namespace {
class DisplayLockTestFindInPageClient : public mojom::blink::FindInPageClient {
 public:
  DisplayLockTestFindInPageClient()
      : find_results_are_ready_(false), active_index_(-1), count_(-1) {}

  ~DisplayLockTestFindInPageClient() override = default;

  void SetFrame(WebLocalFrameImpl* frame) {
    frame->GetFindInPage()->SetClient(receiver_.BindNewPipeAndPassRemote());
  }

  void SetNumberOfMatches(
      int request_id,
      unsigned int current_number_of_matches,
      mojom::blink::FindMatchUpdateType final_update) final {
    count_ = current_number_of_matches;
    find_results_are_ready_ =
        (final_update == mojom::blink::FindMatchUpdateType::kFinalUpdate);
  }

  void SetActiveMatch(int request_id,
                      const gfx::Rect& active_match_rect,
                      int active_match_ordinal,
                      mojom::blink::FindMatchUpdateType final_update) final {
    active_match_rect_ = IntRect(active_match_rect);
    active_index_ = active_match_ordinal;
    find_results_are_ready_ =
        (final_update == mojom::blink::FindMatchUpdateType::kFinalUpdate);
  }

  bool FindResultsAreReady() const { return find_results_are_ready_; }
  int Count() const { return count_; }
  int ActiveIndex() const { return active_index_; }
  IntRect ActiveMatchRect() const { return active_match_rect_; }

  void Reset() {
    find_results_are_ready_ = false;
    count_ = -1;
    active_index_ = -1;
    active_match_rect_ = IntRect();
  }

 private:
  IntRect active_match_rect_;
  bool find_results_are_ready_;
  int active_index_;

  int count_;
  mojo::Receiver<mojom::blink::FindInPageClient> receiver_{this};
};

class DisplayLockEmptyEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) final {}
};
}  // namespace

class DisplayLockContextTest : public testing::Test,
                               private ScopedCSSSubtreeVisibilityHiddenMatchableForTest {
 public:
  DisplayLockContextTest() : ScopedCSSSubtreeVisibilityHiddenMatchableForTest(true) {}

  void SetUp() override {
    web_view_helper_.Initialize();
  }

  void TearDown() override {
    web_view_helper_.Reset();
  }

  Document& GetDocument() {
    return *static_cast<Document*>(
        web_view_helper_.LocalMainFrame()->GetDocument());
  }
  FindInPage* GetFindInPage() {
    return web_view_helper_.LocalMainFrame()->GetFindInPage();
  }
  WebLocalFrameImpl* LocalMainFrame() {
    return web_view_helper_.LocalMainFrame();
  }

  void UpdateAllLifecyclePhasesForTest() {
    GetDocument().View()->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  }

  void SetHtmlInnerHTML(const char* content) {
    GetDocument().documentElement()->setInnerHTML(String::FromUTF8(content));
    UpdateAllLifecyclePhasesForTest();
  }

  void ResizeAndFocus() {
    web_view_helper_.Resize(WebSize(640, 480));
    web_view_helper_.GetWebView()->MainFrameWidget()->SetFocus(true);
    test::RunPendingTasks();
  }

  void LockElement(Element& element, bool activatable) {
    StringBuilder value;
    value.Append("subtree-visibility: hidden");
    if (activatable)
      value.Append("-matchable");
    element.setAttribute(html_names::kStyleAttr, value.ToAtomicString());
    UpdateAllLifecyclePhasesForTest();
  }

  void CommitElement(Element& element, bool update_lifecycle = true) {
    element.setAttribute(html_names::kStyleAttr, "");
    if (update_lifecycle)
      UpdateAllLifecyclePhasesForTest();
  }

  void UnlockImmediate(DisplayLockContext* context) {
    context->SetRequestedState(ESubtreeVisibility::kVisible);
  }

  bool GraphicsLayerNeedsCollection(DisplayLockContext* context) const {
    return context->needs_graphics_layer_collection_;
  }

  mojom::blink::FindOptionsPtr FindOptions(bool find_next = false) {
    auto find_options = mojom::blink::FindOptions::New();
    find_options->run_synchronously_for_testing = true;
    find_options->find_next = find_next;
    find_options->forward = true;
    return find_options;
  }

  void Find(String search_text,
            DisplayLockTestFindInPageClient& client,
            bool find_next = false) {
    client.Reset();
    GetFindInPage()->Find(FAKE_FIND_ID, search_text, FindOptions(find_next));
    test::RunPendingTasks();
  }

  bool ReattachWasBlocked(DisplayLockContext* context) {
    return context->reattach_layout_tree_was_blocked_;
  }

  const int FAKE_FIND_ID = 1;

 private:
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(DisplayLockContextTest, LockAfterAppendStyleDirtyBits) {
  SetHtmlInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body><div id="container"><div id="child"></div></div></body>
  )HTML");

  auto* element = GetDocument().getElementById("container");
  LockElement(*element, false);

  // Finished acquiring the lock.
  // Note that because the element is locked after append, the "self" phase for
  // style should still happen.
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldStyle(
      DisplayLockLifecycleTarget::kSelf));
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyle(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayout(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);

  // If the element is dirty, style recalc would handle it in the next recalc.
  element->setAttribute(
      html_names::kStyleAttr,
      "subtree-visibility: hidden; color: red;");
  EXPECT_TRUE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_TRUE(element->GetComputedStyle());
  EXPECT_EQ(
      element->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()),
      MakeRGB(255, 0, 0));
  // Manually commit the lock so that we can verify which dirty bits get
  // propagated.
  UnlockImmediate(element->GetDisplayLockContext());
  element->setAttribute(html_names::kStyleAttr, "color: red;");

  auto* child = GetDocument().getElementById("child");
  EXPECT_TRUE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(child->NeedsStyleRecalc());
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(child->NeedsStyleRecalc());

  // Lock the child.
  child->setAttribute(
      html_names::kStyleAttr,
      "subtree-visibility: hidden; color: blue;");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(child->NeedsStyleRecalc());
  ASSERT_TRUE(child->GetComputedStyle());
  EXPECT_EQ(
      child->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()),
      MakeRGB(0, 0, 255));

  UnlockImmediate(child->GetDisplayLockContext());
  child->setAttribute(html_names::kStyleAttr, "color: blue;");
  EXPECT_TRUE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_TRUE(element->ChildNeedsStyleRecalc());
  EXPECT_TRUE(child->NeedsStyleRecalc());
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(child->NeedsStyleRecalc());
  ASSERT_TRUE(child->GetComputedStyle());
  EXPECT_EQ(
      child->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()),
      MakeRGB(0, 0, 255));
}

TEST_F(DisplayLockContextTest, LockedElementIsNotSearchableViaFindInPage) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body><div id="container">testing</div></body>
  )HTML");

  const String search_text = "testing";
  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());

  auto* container = GetDocument().getElementById("container");
  LockElement(*container, false /* activatable */);
  Find(search_text, client);
  EXPECT_EQ(0, client.Count());

  // Check if we can find the result after we commit.
  CommitElement(*container);
  Find(search_text, client);
  EXPECT_EQ(1, client.Count());
}

TEST_F(DisplayLockContextTest,
       ActivatableLockedElementIsSearchableViaFindInPage) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    .spacer {
      height: 10000px;
    }
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body><div class=spacer></div><div id="container">testing</div></body>
  )HTML");

  const String search_text = "testing";
  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());

  // Finds on a normal element.
  Find(search_text, client);
  EXPECT_EQ(1, client.Count());
  // Clears selections since we're going to use the same query next time.
  GetFindInPage()->StopFinding(
      mojom::StopFindAction::kStopFindActionClearSelection);

  auto* container = GetDocument().getElementById("container");
  LockElement(*container, true /* activatable */);

  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());
  // Check if we can still get the same result with the same query.
  Find(search_text, client);
  EXPECT_EQ(1, client.Count());
  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());
  EXPECT_GT(GetDocument().scrollingElement()->scrollTop(), 1000);
}

TEST_F(DisplayLockContextTest,
       ActivatableLockedElementTickmarksAreAtLockedRoots) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    body {
      margin: 0;
      padding: 0;
    }
    .small {
      width: 100px;
      height: 100px;
    }
    .medium {
      width: 150px;
      height: 150px;
    }
    .large {
      width: 200px;
      height: 200px;
    }
    </style>
    <body>
      testing
      <div id="container1" class=small>testing</div>
      <div id="container2" class=medium>testing</div>
      <div id="container3" class=large>
        <div id="container4" class=medium>testing</div>
      </div>
      <div id="container5" class=small>testing</div>
    </body>
  )HTML");

  const String search_text = "testing";
  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());

  auto* container1 = GetDocument().getElementById("container1");
  auto* container2 = GetDocument().getElementById("container2");
  auto* container3 = GetDocument().getElementById("container3");
  auto* container4 = GetDocument().getElementById("container4");
  auto* container5 = GetDocument().getElementById("container5");
  LockElement(*container5, false /* activatable */);
  LockElement(*container4, true /* activatable */);
  LockElement(*container3, true /* activatable */);
  LockElement(*container2, true /* activatable */);
  LockElement(*container1, true /* activatable */);

  EXPECT_TRUE(container1->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(container2->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(container3->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(container4->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(container5->GetDisplayLockContext()->IsLocked());

  // Do a find-in-page.
  Find(search_text, client);
  // "testing" outside of the container divs, and 3 inside activatable divs.
  EXPECT_EQ(4, client.Count());

  auto tick_rects = GetDocument().Markers().LayoutRectsForTextMatchMarkers();
  ASSERT_EQ(4u, tick_rects.size());

  // Sort the layout rects by y coordinate for deterministic checks below.
  std::sort(tick_rects.begin(), tick_rects.end(),
            [](const IntRect& a, const IntRect& b) { return a.Y() < b.Y(); });

  int y_offset = tick_rects[0].Height();

  // The first tick rect will be based on the text itself, so we don't need to
  // check that. The next three should be the small, medium and large rects,
  // since those are the locked roots.
  EXPECT_EQ(IntRect(0, y_offset, 100, 100), tick_rects[1]);
  y_offset += tick_rects[1].Height();
  EXPECT_EQ(IntRect(0, y_offset, 150, 150), tick_rects[2]);
  y_offset += tick_rects[2].Height();
  EXPECT_EQ(IntRect(0, y_offset, 200, 200), tick_rects[3]);
}

TEST_F(DisplayLockContextTest,
       FindInPageWhileLockedContentChangesDoesNotCrash) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body>testing<div id="container">testing</div></body>
  )HTML");

  const String search_text = "testing";
  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());

  // Lock the container.
  auto* container = GetDocument().getElementById("container");
  LockElement(*container, true /* activatable */);
  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());

  // Find the first "testing", container still locked since the match is outside
  // the container.
  Find(search_text, client);
  EXPECT_EQ(2, client.Count());
  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());

  // Change the inner text, this should not DCHECK.
  container->setInnerHTML("please don't DCHECK");
  UpdateAllLifecyclePhasesForTest();
}

TEST_F(DisplayLockContextTest, FindInPageWithChangedContent) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body><div id="container">testing</div></body>
  )HTML");

  // Check if the result is correct if we update the contents.
  auto* container = GetDocument().getElementById("container");
  LockElement(*container, true /* activatable */);
  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());
  container->setInnerHTML(
      "testing"
      "<div>testing</div>"
      "tes<div style='display:none;'>x</div>ting");

  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());
  Find("testing", client);
  EXPECT_EQ(3, client.Count());
  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());
}

TEST_F(DisplayLockContextTest, FindInPageWithNoMatchesWontUnlock) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body><div id="container">tes<div>ting</div><div style='display:none;'>testing</div></div></body>
  )HTML");

  auto* container = GetDocument().getElementById("container");
  LockElement(*container, true /* activatable */);
  LockElement(*container, true /* activatable */);
  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());

  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());
  Find("testing", client);
  // No results found, container stays locked.
  EXPECT_EQ(0, client.Count());
  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());
}

TEST_F(DisplayLockContextTest,
       NestedActivatableLockedElementIsSearchableViaFindInPage) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <body>
      <style>
        div {
          width: 100px;
          height: 100px;
          contain: style layout;
        }
      </style>
      <div id='container'>
        <div>testing1</div>
        <div id='activatable'>
        testing2
          <div id='nestedNonActivatable'>
            testing3
          </div>
        </div>
        <div id='nonActivatable'>testing4</div>
      </div>
    "</body>"
  )HTML");

  auto* container = GetDocument().getElementById("container");
  auto* activatable = GetDocument().getElementById("activatable");
  auto* non_activatable = GetDocument().getElementById("nonActivatable");
  auto* nested_non_activatable =
      GetDocument().getElementById("nestedNonActivatable");

  LockElement(*non_activatable, false /* activatable */);
  LockElement(*nested_non_activatable, false /* activatable */);
  LockElement(*activatable, true /* activatable */);
  LockElement(*container, true /* activatable */);

  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(activatable->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(non_activatable->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(nested_non_activatable->GetDisplayLockContext()->IsLocked());

  // We can find testing1 and testing2.
  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());
  Find("testing", client);
  EXPECT_EQ(2, client.Count());
  EXPECT_EQ(1, client.ActiveIndex());

  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(activatable->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(non_activatable->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(nested_non_activatable->GetDisplayLockContext()->IsLocked());
}

TEST_F(DisplayLockContextTest,
       NestedActivatableLockedElementIsNotUnlockedByFindInPage) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <body>
      <style>
        div {
          width: 100px;
          height: 100px;
          contain: style layout;
        }
      </style>
      <div id='container'>
        <div id='child'>testing1</div>
      </div>
  )HTML");
  auto* container = GetDocument().getElementById("container");
  auto* child = GetDocument().getElementById("child");
  LockElement(*child, true /* activatable */);
  LockElement(*container, true /* activatable */);

  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(child->GetDisplayLockContext()->IsLocked());
  // We can find testing1 and testing2.
  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());
  Find("testing", client);
  EXPECT_EQ(1, client.Count());
  EXPECT_EQ(1, client.ActiveIndex());

  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(child->GetDisplayLockContext()->IsLocked());
}

TEST_F(DisplayLockContextTest,
       FindInPageNavigateLockedMatchesRespectsActivatable) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body>
      <div id="container">
        <div id="one">result</div>
        <div id="two"><b>r</b>esult</div>
        <div id="three">r<i>esul</i>t</div>
      </div>
    </body>
  )HTML");

  auto* div_one = GetDocument().getElementById("one");
  auto* div_two = GetDocument().getElementById("two");
  auto* div_three = GetDocument().getElementById("three");
  // Lock three divs, make #div_two non-activatable.
  LockElement(*div_one, true /* activatable */);
  LockElement(*div_two, false /* activatable */);
  LockElement(*div_three, true /* activatable */);

  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());
  WebString search_text(String("result"));

  auto text_rect = [](Element* element) {
    return ComputeTextRect(EphemeralRange::RangeOfContents(*element));
  };

  // Find result in #one.
  Find(search_text, client);
  EXPECT_EQ(2, client.Count());
  EXPECT_EQ(1, client.ActiveIndex());
  EXPECT_EQ(text_rect(div_one), client.ActiveMatchRect());
  // Pretend that the event unlocked the element.
  CommitElement(*div_one);

  // Going forward from #one would go to #three.
  Find(search_text, client, true /* find_next */);
  EXPECT_EQ(2, client.Count());
  EXPECT_EQ(2, client.ActiveIndex());
  EXPECT_EQ(text_rect(div_three), client.ActiveMatchRect());
  // Pretend that the event unlocked the element.
  CommitElement(*div_three);

  // Going backwards from #three would go to #one.
  client.Reset();
  auto find_options = FindOptions();
  find_options->forward = false;
  GetFindInPage()->Find(FAKE_FIND_ID, search_text, find_options->Clone());
  test::RunPendingTasks();
  EXPECT_EQ(2, client.Count());
  EXPECT_EQ(1, client.ActiveIndex());
  EXPECT_EQ(text_rect(div_one), client.ActiveMatchRect());
}

TEST_F(DisplayLockContextTest, CallUpdateStyleAndLayoutAfterChange) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body><div id="container"><b>t</b>esting</div></body>
  )HTML");
  auto* element = GetDocument().getElementById("container");
  LockElement(*element, false);

  // Sanity checks to ensure the element is locked.
  EXPECT_TRUE(element->GetDisplayLockContext()->IsLocked());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyle(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayout(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 1);

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  // Testing whitespace reattachment, shouldn't mark for reattachment.
  element->firstChild()->remove();

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  // Testing whitespace reattachment + dirty style.
  element->setInnerHTML("<div>something</div>");

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_TRUE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_TRUE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  // Manually start commit, so that we can verify which dirty bits get
  // propagated.
  UnlockImmediate(element->GetDisplayLockContext());
  EXPECT_TRUE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  // Simulating style recalc happening, will mark for reattachment.
  element->ClearChildNeedsStyleRecalc();
  element->firstChild()->ClearNeedsStyleRecalc();
  element->GetDisplayLockContext()->DidStyle(
      DisplayLockLifecycleTarget::kChildren);

  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_TRUE(element->ChildNeedsReattachLayoutTree());
}

TEST_F(DisplayLockContextTest, CallUpdateStyleAndLayoutAfterChangeCSS) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    .bg {
      background: blue;
    }
    .locked {
      subtree-visibility: hidden;
    }
    </style>
    <body><div class=locked id="container"><b>t</b>esting<div id=inner></div></div></body>
  )HTML");
  auto* element = GetDocument().getElementById("container");
  auto* inner = GetDocument().getElementById("inner");

  // Sanity checks to ensure the element is locked.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyle(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayout(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 1);

  EXPECT_TRUE(ReattachWasBlocked(element->GetDisplayLockContext()));
  // Note that we didn't create a layout object for inner, since the layout tree
  // attachment was blocked.
  EXPECT_FALSE(inner->GetLayoutObject());

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  element->classList().Remove("locked");

  // Class list changed, so we should need self style change.
  EXPECT_TRUE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());
  // Because we upgraded our style change, we created a layout object for inner.
  EXPECT_TRUE(inner->GetLayoutObject());
}

TEST_F(DisplayLockContextTest, LockedElementAndDescendantsAreNotFocusable) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body>
    <div id="container">
      <input id="textfield", type="text">
    </div>
    </body>
  )HTML");

  // We start off as being focusable.
  ASSERT_TRUE(GetDocument().getElementById("textfield")->IsKeyboardFocusable());
  ASSERT_TRUE(GetDocument().getElementById("textfield")->IsMouseFocusable());
  ASSERT_TRUE(GetDocument().getElementById("textfield")->IsFocusable());
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 0);

  auto* element = GetDocument().getElementById("container");
  LockElement(*element, false);

  // Sanity checks to ensure the element is locked.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyle(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayout(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 1);

  // The input should not be focusable now.
  EXPECT_FALSE(
      GetDocument().getElementById("textfield")->IsKeyboardFocusable());
  EXPECT_FALSE(GetDocument().getElementById("textfield")->IsMouseFocusable());
  EXPECT_FALSE(GetDocument().getElementById("textfield")->IsFocusable());

  // Calling explicit focus() should also not focus the element.
  GetDocument().getElementById("textfield")->focus();
  EXPECT_FALSE(GetDocument().FocusedElement());

  // Now commit the lock and ensure we can focus the input
  CommitElement(*element);

  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldStyle(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldLayout(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldPaint(
      DisplayLockLifecycleTarget::kChildren));

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 0);
  EXPECT_TRUE(GetDocument().getElementById("textfield")->IsKeyboardFocusable());
  EXPECT_TRUE(GetDocument().getElementById("textfield")->IsMouseFocusable());
  EXPECT_TRUE(GetDocument().getElementById("textfield")->IsFocusable());

  // Calling explicit focus() should focus the element
  GetDocument().getElementById("textfield")->focus();
  EXPECT_EQ(GetDocument().FocusedElement(),
            GetDocument().getElementById("textfield"));
}

TEST_F(DisplayLockContextTest, DisplayLockPreventsActivation) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <body>
    <div id="shadowHost">
      <div id="slotted"></div>
    </div>
    </body>
  )HTML");

  auto* host = GetDocument().getElementById("shadowHost");
  auto* slotted = GetDocument().getElementById("slotted");

  ASSERT_FALSE(
      host->DisplayLockPreventsActivation(DisplayLockActivationReason::kAny));
  ASSERT_FALSE(slotted->DisplayLockPreventsActivation(
      DisplayLockActivationReason::kAny));

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML(
      "<div id='container' style='contain:style layout "
      "paint;'><slot></slot></div>");
  UpdateAllLifecyclePhasesForTest();

  auto* container = shadow_root.getElementById("container");
  EXPECT_FALSE(
      host->DisplayLockPreventsActivation(DisplayLockActivationReason::kAny));
  EXPECT_FALSE(container->DisplayLockPreventsActivation(
      DisplayLockActivationReason::kAny));
  EXPECT_FALSE(slotted->DisplayLockPreventsActivation(
      DisplayLockActivationReason::kAny));

  LockElement(*container, false);

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 1);
  EXPECT_FALSE(
      host->DisplayLockPreventsActivation(DisplayLockActivationReason::kAny));
  EXPECT_TRUE(container->DisplayLockPreventsActivation(
      DisplayLockActivationReason::kAny));
  EXPECT_TRUE(slotted->DisplayLockPreventsActivation(
      DisplayLockActivationReason::kAny));

  // Ensure that we resolve the acquire callback, thus finishing the acquire
  // step.
  UpdateAllLifecyclePhasesForTest();

  CommitElement(*container);

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 0);
  EXPECT_FALSE(
      host->DisplayLockPreventsActivation(DisplayLockActivationReason::kAny));
  EXPECT_FALSE(container->DisplayLockPreventsActivation(
      DisplayLockActivationReason::kAny));
  EXPECT_FALSE(slotted->DisplayLockPreventsActivation(
      DisplayLockActivationReason::kAny));

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 0);
  EXPECT_FALSE(
      host->DisplayLockPreventsActivation(DisplayLockActivationReason::kAny));
  EXPECT_FALSE(container->DisplayLockPreventsActivation(
      DisplayLockActivationReason::kAny));
  EXPECT_FALSE(slotted->DisplayLockPreventsActivation(
      DisplayLockActivationReason::kAny));

  SetHtmlInnerHTML(R"HTML(
    <body>
    <div id="nonviewport" style="subtree-visibility: hidden-matchable">
    </div>
    </body>
  )HTML");
  auto* non_viewport = GetDocument().getElementById("nonviewport");

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 0);

  EXPECT_FALSE(non_viewport->DisplayLockPreventsActivation(
      DisplayLockActivationReason::kAny));
  EXPECT_FALSE(non_viewport->DisplayLockPreventsActivation(
      DisplayLockActivationReason::kFindInPage));
  EXPECT_TRUE(non_viewport->DisplayLockPreventsActivation(
      DisplayLockActivationReason::kUserFocus));
}

TEST_F(DisplayLockContextTest,
       LockedElementAndFlatTreeDescendantsAreNotFocusable) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <body>
    <div id="shadowHost">
      <input id="textfield" type="text">
    </div>
    </body>
  )HTML");

  auto* host = GetDocument().getElementById("shadowHost");
  auto* text_field = GetDocument().getElementById("textfield");
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML(
      "<div id='container' style='contain:style layout "
      "paint;'><slot></slot></div>");

  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(text_field->IsKeyboardFocusable());
  ASSERT_TRUE(text_field->IsMouseFocusable());
  ASSERT_TRUE(text_field->IsFocusable());

  auto* element = shadow_root.getElementById("container");
  LockElement(*element, false);

  // Sanity checks to ensure the element is locked.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyle(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayout(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint(
      DisplayLockLifecycleTarget::kChildren));
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 1);

  // The input should not be focusable now.
  EXPECT_FALSE(text_field->IsKeyboardFocusable());
  EXPECT_FALSE(text_field->IsMouseFocusable());
  EXPECT_FALSE(text_field->IsFocusable());

  // Calling explicit focus() should also not focus the element.
  text_field->focus();
  EXPECT_FALSE(GetDocument().FocusedElement());
}

TEST_F(DisplayLockContextTest, LockedCountsWithMultipleLocks) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    .container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body>
    <div id="one" class="container">
      <div id="two" class="container"></div>
    </div>
    <div id="three" class="container"></div>
    </body>
  )HTML");

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 0);

  auto* one = GetDocument().getElementById("one");
  auto* two = GetDocument().getElementById("two");
  auto* three = GetDocument().getElementById("three");

  LockElement(*one, false);

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 1);

  LockElement(*two, false);

  // Because |two| is nested, the lock counts aren't updated since the lock
  // doesn't actually take effect until style can determine that we should lock.
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 1);

  LockElement(*three, false);

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 2);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 2);

  // Now commit the outer lock.
  CommitElement(*one);

  // The counts remain the same since now the inner lock is determined to be
  // locked.
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 2);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 2);

  // Commit the inner lock.
  CommitElement(*two);

  // Both inner and outer locks should have committed.
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 1);

  // Commit the sibling lock.
  CommitElement(*three);

  // Both inner and outer locks should have committed.
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 0);
}

TEST_F(DisplayLockContextTest, ActivatableNotCountedAsBlocking) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    .container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body>
    <div id="activatable" class="container"></div>
    <div id="nonActivatable" class="container"></div>
    </body>
  )HTML");

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 0);

  auto* activatable = GetDocument().getElementById("activatable");
  auto* non_activatable = GetDocument().getElementById("nonActivatable");

  // Initial display lock context should be activatable, since nothing skipped
  // activation for it.
  EXPECT_TRUE(activatable->EnsureDisplayLockContext().IsActivatable(
      DisplayLockActivationReason::kAny));

  LockElement(*activatable, true);

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 0);
  EXPECT_TRUE(activatable->GetDisplayLockContext()->IsActivatable(
      DisplayLockActivationReason::kAny));

  LockElement(*non_activatable, false);

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 2);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 1);
  EXPECT_FALSE(non_activatable->GetDisplayLockContext()->IsActivatable(
      DisplayLockActivationReason::kAny));

  // Now commit the lock for |non_activatable|.
  CommitElement(*non_activatable);

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 0);

  // Re-acquire the lock for |activatable| again with the activatable flag.
  LockElement(*activatable, true);

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 0);
  EXPECT_TRUE(activatable->GetDisplayLockContext()->IsActivatable(
      DisplayLockActivationReason::kAny));
}

TEST_F(DisplayLockContextTest, ElementInTemplate) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #child {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    #grandchild {
      color: blue;
    }
    #container {
      display: none;
    }
    </style>
    <body>
      <template id="template"><div id="child"><div id="grandchild">foo</div></div></template>
      <div id="container"></div>
    </body>
  )HTML");

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 0);

  auto* template_el =
      To<HTMLTemplateElement>(GetDocument().getElementById("template"));
  auto* child = To<Element>(template_el->content()->firstChild());
  EXPECT_FALSE(child->isConnected());

  // Try to lock an element in a template.
  LockElement(*child, false);

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 0);
  EXPECT_FALSE(child->GetDisplayLockContext());

  // Commit also works, but does nothing.
  CommitElement(*child);
  EXPECT_FALSE(child->GetDisplayLockContext());

  // Try to lock an element that was moved from a template to a document.
  auto* document_child =
      To<Element>(GetDocument().adoptNode(child, ASSERT_NO_EXCEPTION));
  auto* container = GetDocument().getElementById("container");
  container->appendChild(document_child);

  LockElement(*document_child, false);

  // These should be 0, since container is display: none, so locking its child
  // is not visible to style.
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 0);
  ASSERT_FALSE(document_child->GetDisplayLockContext());

  container->setAttribute(html_names::kStyleAttr, "display: block;");
  EXPECT_TRUE(container->NeedsStyleRecalc());
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 1);
  ASSERT_TRUE(document_child->GetDisplayLockContext());
  EXPECT_TRUE(document_child->GetDisplayLockContext()->IsLocked());

  document_child->setAttribute(
      html_names::kStyleAttr,
      "subtree-visibility: hidden; color: red;");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document_child->NeedsStyleRecalc());

  // Commit will unlock the element and update the style.
  document_child->setAttribute(html_names::kStyleAttr, "color: red;");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document_child->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().DisplayLockBlockingAllActivationCount(), 0);

  EXPECT_FALSE(document_child->NeedsStyleRecalc());
  EXPECT_FALSE(document_child->ChildNeedsStyleRecalc());
  ASSERT_TRUE(document_child->GetComputedStyle());
  EXPECT_EQ(document_child->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()),
            MakeRGB(255, 0, 0));

  auto* grandchild = GetDocument().getElementById("grandchild");
  EXPECT_FALSE(grandchild->NeedsStyleRecalc());
  EXPECT_FALSE(grandchild->ChildNeedsStyleRecalc());
  ASSERT_TRUE(grandchild->GetComputedStyle());
  EXPECT_EQ(grandchild->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()),
            MakeRGB(0, 0, 255));
}

TEST_F(DisplayLockContextTest, AncestorAllowedTouchAction) {
  SetHtmlInnerHTML(R"HTML(
    <style>
    #locked {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <div id="ancestor">
      <div id="handler">
        <div id="descendant">
          <div id="locked">
            <div id="lockedchild"></div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  auto* ancestor_element = GetDocument().getElementById("ancestor");
  auto* handler_element = GetDocument().getElementById("handler");
  auto* descendant_element = GetDocument().getElementById("descendant");
  auto* locked_element = GetDocument().getElementById("locked");
  auto* lockedchild_element = GetDocument().getElementById("lockedchild");

  LockElement(*locked_element, false);
  EXPECT_TRUE(locked_element->GetDisplayLockContext()->IsLocked());

  auto* ancestor_object = ancestor_element->GetLayoutObject();
  auto* handler_object = handler_element->GetLayoutObject();
  auto* descendant_object = descendant_element->GetLayoutObject();
  auto* locked_object = locked_element->GetLayoutObject();
  auto* lockedchild_object = lockedchild_element->GetLayoutObject();

  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(lockedchild_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      lockedchild_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(handler_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(lockedchild_object->InsideBlockingTouchEventHandler());

  auto* callback = MakeGarbageCollected<DisplayLockEmptyEventListener>();
  handler_element->addEventListener(event_type_names::kTouchstart, callback);

  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(handler_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(lockedchild_object->EffectiveAllowedTouchActionChanged());

  EXPECT_TRUE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      lockedchild_object->DescendantEffectiveAllowedTouchActionChanged());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(lockedchild_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      lockedchild_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(handler_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(lockedchild_object->InsideBlockingTouchEventHandler());

  // Manually commit the lock so that we can verify which dirty bits get
  // propagated.
  CommitElement(*locked_element, false);
  UnlockImmediate(locked_element->GetDisplayLockContext());

  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(lockedchild_object->EffectiveAllowedTouchActionChanged());

  EXPECT_TRUE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(handler_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      lockedchild_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(handler_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(lockedchild_object->InsideBlockingTouchEventHandler());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(lockedchild_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      lockedchild_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(handler_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(lockedchild_object->InsideBlockingTouchEventHandler());
}

TEST_F(DisplayLockContextTest, DescendantAllowedTouchAction) {
  SetHtmlInnerHTML(R"HTML(
    <style>
    #locked {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <div id="ancestor">
      <div id="descendant">
        <div id="locked">
          <div id="handler"></div>
        </div>
      </div>
    </div>
  )HTML");

  auto* ancestor_element = GetDocument().getElementById("ancestor");
  auto* descendant_element = GetDocument().getElementById("descendant");
  auto* locked_element = GetDocument().getElementById("locked");
  auto* handler_element = GetDocument().getElementById("handler");

  LockElement(*locked_element, false);
  EXPECT_TRUE(locked_element->GetDisplayLockContext()->IsLocked());

  auto* ancestor_object = ancestor_element->GetLayoutObject();
  auto* descendant_object = descendant_element->GetLayoutObject();
  auto* locked_object = locked_element->GetLayoutObject();
  auto* handler_object = handler_element->GetLayoutObject();

  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(handler_object->InsideBlockingTouchEventHandler());

  auto* callback = MakeGarbageCollected<DisplayLockEmptyEventListener>();
  handler_element->addEventListener(event_type_names::kTouchstart, callback);

  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(handler_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(handler_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(handler_object->InsideBlockingTouchEventHandler());

  // Do the same check again. For now, nothing is expected to change. However,
  // when we separate self and child layout, then some flags would be different.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(handler_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(handler_object->InsideBlockingTouchEventHandler());

  // Manually commit the lock so that we can verify which dirty bits get
  // propagated.
  CommitElement(*locked_element, false);
  UnlockImmediate(locked_element->GetDisplayLockContext());

  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(handler_object->EffectiveAllowedTouchActionChanged());

  EXPECT_TRUE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(handler_object->InsideBlockingTouchEventHandler());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(handler_object->InsideBlockingTouchEventHandler());
}

TEST_F(DisplayLockContextTest,
       CompositedLayerLockCausesGraphicsLayersCollection) {
  ResizeAndFocus();
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);

  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: style layout;
      will-change: transform;
    }
    #composited {
      will-change: transform;
    }
    </style>
    <body>
    <div id="container"><div id="composited">testing</div></div></body>
    </body>
  )HTML");

  // Check if the result is correct if we update the contents.
  auto* container = GetDocument().getElementById("container");

  // Ensure that we will gather graphics layer on the next update (after lock).
  GetDocument().View()->SetForeignLayerListNeedsUpdate();

  LockElement(*container, false /* activatable */);
  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(GraphicsLayerNeedsCollection(container->GetDisplayLockContext()));

  CommitElement(*container);
  EXPECT_FALSE(
      GraphicsLayerNeedsCollection(container->GetDisplayLockContext()));
}

TEST_F(DisplayLockContextTest, DescendantNeedsPaintPropertyUpdateBlocked) {
  SetHtmlInnerHTML(R"HTML(
    <style>
    #locked {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <div id="ancestor">
      <div id="descendant">
        <div id="locked">
          <div id="handler"></div>
        </div>
      </div>
    </div>
  )HTML");

  auto* ancestor_element = GetDocument().getElementById("ancestor");
  auto* descendant_element = GetDocument().getElementById("descendant");
  auto* locked_element = GetDocument().getElementById("locked");
  auto* handler_element = GetDocument().getElementById("handler");

  LockElement(*locked_element, false);
  EXPECT_TRUE(locked_element->GetDisplayLockContext()->IsLocked());

  auto* ancestor_object = ancestor_element->GetLayoutObject();
  auto* descendant_object = descendant_element->GetLayoutObject();
  auto* locked_object = locked_element->GetLayoutObject();
  auto* handler_object = handler_element->GetLayoutObject();

  EXPECT_FALSE(ancestor_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(locked_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->NeedsPaintPropertyUpdate());

  EXPECT_FALSE(ancestor_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(locked_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->DescendantNeedsPaintPropertyUpdate());

  handler_object->SetNeedsPaintPropertyUpdate();

  EXPECT_FALSE(ancestor_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(locked_object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(handler_object->NeedsPaintPropertyUpdate());

  EXPECT_TRUE(ancestor_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(descendant_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(locked_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->DescendantNeedsPaintPropertyUpdate());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(ancestor_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(locked_object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(handler_object->NeedsPaintPropertyUpdate());

  EXPECT_FALSE(ancestor_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(locked_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->DescendantNeedsPaintPropertyUpdate());

  locked_object->SetShouldCheckForPaintInvalidationWithoutGeometryChange();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(ancestor_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(locked_object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(handler_object->NeedsPaintPropertyUpdate());

  EXPECT_FALSE(ancestor_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(locked_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->DescendantNeedsPaintPropertyUpdate());

  // Manually commit the lock so that we can verify which dirty bits get
  // propagated.
  CommitElement(*locked_element, false);
  UnlockImmediate(locked_element->GetDisplayLockContext());

  EXPECT_FALSE(ancestor_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(locked_object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(handler_object->NeedsPaintPropertyUpdate());

  EXPECT_TRUE(ancestor_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(descendant_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(locked_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->DescendantNeedsPaintPropertyUpdate());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(ancestor_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(locked_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->NeedsPaintPropertyUpdate());

  EXPECT_FALSE(ancestor_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(locked_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->DescendantNeedsPaintPropertyUpdate());
}

class DisplayLockContextRenderingTest : public RenderingTest,
                                        private ScopedCSSSubtreeVisibilityHiddenMatchableForTest {
 public:
  DisplayLockContextRenderingTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()),
        ScopedCSSSubtreeVisibilityHiddenMatchableForTest(true) {}

  bool IsObservingLifecycle(DisplayLockContext* context) const {
    return context->is_registered_for_lifecycle_notifications_;
  }
  void LockImmediate(DisplayLockContext* context) {
    context->SetRequestedState(ESubtreeVisibility::kHidden);
  }
  void RunStartOfLifecycleTasks() {
    auto start_of_lifecycle_tasks =
        GetDocument().View()->TakeStartOfLifecycleTasksForTest();
    for (auto& task : start_of_lifecycle_tasks)
      std::move(task).Run();
  }
};

TEST_F(DisplayLockContextRenderingTest, FrameDocumentRemovedWhileAcquire) {
  SetHtmlInnerHTML(R"HTML(
    <iframe id="frame"></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      div {
        contain: style layout;
      }
    </style>
    <div id="target"></target>
  )HTML");

  auto* target = ChildDocument().getElementById("target");
  GetDocument().getElementById("frame")->remove();

  LockImmediate(&target->EnsureDisplayLockContext());
}

TEST_F(DisplayLockContextRenderingTest,
       SelectionOnAnonymousColumnSpannerDoesNotCrash) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      #columns {
        column-count: 5;
      }
      #spanner {
        column-span: all;
      }
    </style>
    <div id="columns">
      <div id="spanner"></div>
    </div>
  )HTML");

  auto* columns_object =
      GetDocument().getElementById("columns")->GetLayoutObject();
  LayoutObject* spanner_placeholder_object = nullptr;
  for (auto* candidate = columns_object->SlowFirstChild(); candidate;
       candidate = candidate->NextSibling()) {
    if (candidate->IsLayoutMultiColumnSpannerPlaceholder()) {
      spanner_placeholder_object = candidate;
      break;
    }
  }

  ASSERT_TRUE(spanner_placeholder_object);
  EXPECT_FALSE(spanner_placeholder_object->CanBeSelectionLeaf());
}

TEST_F(DisplayLockContextRenderingTest, ObjectsNeedingLayoutConsidersLocks) {
  SetHtmlInnerHTML(R"HTML(
    <div id=a>
      <div id=b>
        <div id=c></div>
        <div id=d></div>
      </div>
      <div id=e>
        <div id=f></div>
        <div id=g></div>
      </div>
    </div>
  )HTML");

  // Dirty all of the leaf nodes.
  auto dirty_all = [this]() {
    GetDocument().getElementById("c")->GetLayoutObject()->SetNeedsLayout(
        "test");
    GetDocument().getElementById("d")->GetLayoutObject()->SetNeedsLayout(
        "test");
    GetDocument().getElementById("f")->GetLayoutObject()->SetNeedsLayout(
        "test");
    GetDocument().getElementById("g")->GetLayoutObject()->SetNeedsLayout(
        "test");
  };

  unsigned dirty_count = 0;
  unsigned total_count = 0;
  bool is_subtree = false;

  dirty_all();
  GetDocument().View()->CountObjectsNeedingLayout(dirty_count, total_count,
                                                  is_subtree);
  // 7 divs + body + html + layout view
  EXPECT_EQ(dirty_count, 10u);
  EXPECT_EQ(total_count, 10u);

  GetDocument().getElementById("e")->setAttribute(html_names::kStyleAttr,
                                                  "subtree-visibility: auto");
  UpdateAllLifecyclePhasesForTest();

  // Note that the dirty_all call propagate the dirty bit from the unlocked
  // subtree all the way up to the layout view, so everything on the way up is
  // dirtied.
  dirty_all();
  GetDocument().View()->CountObjectsNeedingLayout(dirty_count, total_count,
                                                  is_subtree);
  // Element with 2 children is locked, and it itself isn't dirty (just the
  // children are). So, 10 - 3 = 7
  EXPECT_EQ(dirty_count, 7u);
  // We still see the locked element, so the total is 8.
  EXPECT_EQ(total_count, 8u);

  GetDocument().getElementById("a")->setAttribute(html_names::kStyleAttr,
                                                  "subtree-visibility: auto");
  UpdateAllLifecyclePhasesForTest();

  // Note that this dirty_all call is now not propagating the dirty bits at all,
  // since they are stopped at the top level div.
  dirty_all();
  GetDocument().View()->CountObjectsNeedingLayout(dirty_count, total_count,
                                                  is_subtree);
  // Top level element is locked and the dirty bits were not propagated, so we
  // expect 0 dirty elements. The total should be 4 ('a' + body + html + layout
  // view);
  EXPECT_EQ(dirty_count, 0u);
  EXPECT_EQ(total_count, 4u);
}

TEST_F(DisplayLockContextRenderingTest,
       NestedLockDoesNotInvalidateOnHideOrShow) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .auto { subtree-visibility: auto; }
      .hidden { subtree-visibility: hidden; }
      .item { height: 10px; }
      /* this is important to not invalidate layout when we hide the element! */
      #outer { contain: style layout; }
    </style>
    <div id=outer>
      <div id=unrelated>
        <div id=inner class=auto>Content</div>
      </div>
    </div>
  )HTML");

  auto* inner_element = GetDocument().getElementById("inner");
  auto* unrelated_element = GetDocument().getElementById("unrelated");
  auto* outer_element = GetDocument().getElementById("outer");

  // Since visibility switch happens at the start of the next lifecycle, we
  // should have clean layout for now.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsLayout());

  RunStartOfLifecycleTasks();

  // Now that the intersection observer notifications switch the visibility of
  // the context, we expect to see dirty layout bits to be propagated.
  EXPECT_TRUE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_TRUE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_TRUE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsLayout());

  // Clear the layout.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsLayout());

  // Verify lock state.
  auto* inner_context = inner_element->GetDisplayLockContext();
  ASSERT_TRUE(inner_context);
  EXPECT_FALSE(inner_context->IsLocked());

  // Lock outer.
  outer_element->setAttribute(html_names::kClassAttr, "hidden");
  // Ensure the lock processes (but don't run intersection observation tasks
  // yet).
  UpdateAllLifecyclePhasesForTest();

  // Verify the lock exists.
  auto* outer_context = outer_element->GetDisplayLockContext();
  ASSERT_TRUE(outer_context);
  EXPECT_TRUE(outer_context->IsLocked());

  // Everything should be layout clean.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsLayout());

  // Inner context should not be observing the lifecycle.
  EXPECT_FALSE(IsObservingLifecycle(inner_context));

  // Process any visibility changes.
  RunStartOfLifecycleTasks();

  // Run the following checks a few times since we should be observing
  // lifecycle.
  for (int i = 0; i < 3; ++i) {
    // It shouldn't change the fact that we're layout clean.
    EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
    EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsLayout());
    EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
    EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsLayout());
    EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
    EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsLayout());

    // Because we skipped hiding the element, inner_context should be observing
    // lifecycle.
    EXPECT_TRUE(IsObservingLifecycle(inner_context));

    UpdateAllLifecyclePhasesForTest();
  }

  // Unlock outer.
  outer_element->setAttribute(html_names::kClassAttr, "");
  // Ensure the lock processes (but don't run intersection observation tasks
  // yet).
  UpdateAllLifecyclePhasesForTest();

  // Note that although we're not nested, we're still observing the lifecycle
  // because we don't yet know whether we should or should not hide and we only
  // make this decision _before_ the lifecycle actually unlocked outer.
  EXPECT_TRUE(IsObservingLifecycle(inner_context));

  // Verify the lock is gone.
  EXPECT_FALSE(outer_context->IsLocked());

  // Everything should be layout clean.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsLayout());

  // Process visibility changes.
  RunStartOfLifecycleTasks();

  // We now should know we're visible and so we're not observing the lifecycle.
  EXPECT_FALSE(IsObservingLifecycle(inner_context));

  // Also we should still be activated and unlocked.
  EXPECT_FALSE(inner_context->IsLocked());

  // Everything should be layout clean.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsLayout());
}

TEST_F(DisplayLockContextRenderingTest, NestedLockDoesHideWhenItIsOffscreen) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .auto { subtree-visibility: auto; }
      .hidden { subtree-visibility: hidden; }
      .item { height: 10px; }
      /* this is important to not invalidate layout when we hide the element! */
      #outer { contain: style layout; }
      .spacer { height: 10000px; }
    </style>
    <div id=future_spacer></div>
    <div id=outer>
      <div id=unrelated>
        <div id=inner class=auto>Content</div>
      </div>
    </div>
  )HTML");

  auto* inner_element = GetDocument().getElementById("inner");
  auto* unrelated_element = GetDocument().getElementById("unrelated");
  auto* outer_element = GetDocument().getElementById("outer");

  // Since visibility switch happens at the start of the next lifecycle, we
  // should have clean layout for now.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsLayout());

  RunStartOfLifecycleTasks();

  // Now that the intersection observer notifications switch the visibility of
  // the context, we expect to see dirty layout bits to be propagated.
  EXPECT_TRUE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_TRUE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_TRUE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsLayout());

  // Clear the layout.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsLayout());

  // Verify lock state.
  auto* inner_context = inner_element->GetDisplayLockContext();
  ASSERT_TRUE(inner_context);
  EXPECT_FALSE(inner_context->IsLocked());

  // Lock outer.
  outer_element->setAttribute(html_names::kClassAttr, "hidden");
  // Ensure the lock processes (but don't run intersection observation tasks
  // yet).
  UpdateAllLifecyclePhasesForTest();

  // Verify the lock exists.
  auto* outer_context = outer_element->GetDisplayLockContext();
  ASSERT_TRUE(outer_context);
  EXPECT_TRUE(outer_context->IsLocked());

  // Everything should be layout clean.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsLayout());

  // Inner context should not be observing the lifecycle.
  EXPECT_FALSE(IsObservingLifecycle(inner_context));

  // Process any visibility changes.
  RunStartOfLifecycleTasks();

  // It shouldn't change the fact that we're layout clean.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsLayout());

  // Let future spacer become a real spacer!
  GetDocument()
      .getElementById("future_spacer")
      ->setAttribute(html_names::kClassAttr, "spacer");

  UpdateAllLifecyclePhasesForTest();

  // Because we skipped hiding the element, inner_context should be observing
  // lifecycle.
  EXPECT_TRUE(IsObservingLifecycle(inner_context));

  // Unlock outer.
  outer_element->setAttribute(html_names::kClassAttr, "");
  // Ensure the lock processes (but don't run intersection observation tasks
  // yet).
  UpdateAllLifecyclePhasesForTest();

  // Note that although we're not nested, we're still observing the lifecycle
  // because we don't yet know whether we should or should not hide and we only
  // make this decision _before_ the lifecycle actually unlocked outer.
  EXPECT_TRUE(IsObservingLifecycle(inner_context));

  // Verify the lock is gone.
  EXPECT_FALSE(outer_context->IsLocked());

  // Everything should be layout clean.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsLayout());

  // Process any visibility changes.
  RunStartOfLifecycleTasks();

  // We're still invisible, and we don't know that we're not nested so we're
  // still observing the lifecycle.
  EXPECT_TRUE(IsObservingLifecycle(inner_context));

  // We're unlocked for now.
  EXPECT_FALSE(inner_context->IsLocked());

  // Everything should be layout clean.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsLayout());

  UpdateAllLifecyclePhasesForTest();

  // We figured out that we're actually invisible so no need to observe the
  // lifecycle.
  EXPECT_FALSE(IsObservingLifecycle(inner_context));

  // We're locked.
  EXPECT_TRUE(inner_context->IsLocked());
}
}  // namespace blink
