// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_object.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {
namespace test {

class AccessibilityLayoutTest : public testing::WithParamInterface<bool>,
                                private ScopedLayoutNGForTest,
                                public AccessibilityTest {
 public:
  AccessibilityLayoutTest() : ScopedLayoutNGForTest(GetParam()) {}

 protected:
  bool LayoutNGEnabled() const {
    return RuntimeEnabledFeatures::LayoutNGEnabled();
  }
};

INSTANTIATE_TEST_SUITE_P(AccessibilityTest,
                         AccessibilityLayoutTest,
                         testing::Bool());

TEST_F(AccessibilityTest, IsDescendantOf) {
  SetBodyInnerHTML(R"HTML(<button id="button">button</button>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);

  EXPECT_TRUE(button->IsDescendantOf(*root));
  EXPECT_FALSE(root->IsDescendantOf(*root));
  EXPECT_FALSE(button->IsDescendantOf(*button));
  EXPECT_FALSE(root->IsDescendantOf(*button));
}

TEST_F(AccessibilityTest, IsAncestorOf) {
  SetBodyInnerHTML(R"HTML(<button id="button">button</button>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);

  EXPECT_TRUE(root->IsAncestorOf(*button));
  EXPECT_FALSE(root->IsAncestorOf(*root));
  EXPECT_FALSE(button->IsAncestorOf(*button));
  EXPECT_FALSE(button->IsAncestorOf(*root));
}

TEST_F(AccessibilityTest, UnignoredChildren) {
  SetBodyInnerHTML(R"HTML(This is a test with
                   <p role="presentation">
                     ignored objects
                   </p>
                   <p>
                     which are at multiple
                   </p>
                   <p role="presentation">
                     <p role="presentation">
                       depth levels
                     </p>
                     in the accessibility tree.
                   </p>)HTML");

  const AXObject* ax_body = GetAXRootObject()->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_body);

  ASSERT_EQ(5, ax_body->UnignoredChildCount());
  EXPECT_EQ(ax::mojom::blink::Role::kStaticText,
            ax_body->UnignoredChildAt(0)->RoleValue());
  EXPECT_EQ("This is a test with",
            ax_body->UnignoredChildAt(0)->ComputedName());
  EXPECT_EQ(ax::mojom::blink::Role::kStaticText,
            ax_body->UnignoredChildAt(1)->RoleValue());
  EXPECT_EQ("ignored objects", ax_body->UnignoredChildAt(1)->ComputedName());
  EXPECT_EQ(ax::mojom::blink::Role::kParagraph,
            ax_body->UnignoredChildAt(2)->RoleValue());
  EXPECT_EQ(ax::mojom::blink::Role::kStaticText,
            ax_body->UnignoredChildAt(3)->RoleValue());
  EXPECT_EQ("depth levels", ax_body->UnignoredChildAt(3)->ComputedName());
  EXPECT_EQ(ax::mojom::blink::Role::kStaticText,
            ax_body->UnignoredChildAt(4)->RoleValue());
  EXPECT_EQ("in the accessibility tree.",
            ax_body->UnignoredChildAt(4)->ComputedName());
}

TEST_F(AccessibilityTest, SimpleTreeNavigation) {
  SetBodyInnerHTML(R"HTML(<input id="input" type="text" value="value">
                   <div id="ignored_a" aria-hidden="true"></div>
                   <p id="paragraph">hello<br id="br">there</p>
                   <span id="ignored_b" aria-hidden="true"></span>
                   <button id="button">button</button>)HTML");

  const AXObject* body = GetAXBodyObject();
  ASSERT_NE(nullptr, body);
  const AXObject* input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, input);
  ASSERT_NE(nullptr, GetAXObjectByElementId("ignored_a"));
  ASSERT_TRUE(GetAXObjectByElementId("ignored_a")->AccessibilityIsIgnored());
  const AXObject* paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, paragraph);
  const AXObject* br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, br);
  ASSERT_NE(nullptr, GetAXObjectByElementId("ignored_b"));
  ASSERT_TRUE(GetAXObjectByElementId("ignored_b")->AccessibilityIsIgnored());
  const AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);

  EXPECT_EQ(input, body->FirstChildIncludingIgnored());
  EXPECT_EQ(button, body->LastChildIncludingIgnored());

  ASSERT_NE(nullptr, paragraph->FirstChildIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            paragraph->FirstChildIncludingIgnored()->RoleValue());
  ASSERT_NE(nullptr, paragraph->LastChildIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            paragraph->LastChildIncludingIgnored()->RoleValue());
  ASSERT_NE(nullptr, paragraph->DeepestFirstChildIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            paragraph->DeepestFirstChildIncludingIgnored()->RoleValue());
  ASSERT_NE(nullptr, paragraph->DeepestLastChildIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            paragraph->DeepestLastChildIncludingIgnored()->RoleValue());

  EXPECT_EQ(paragraph->PreviousSiblingIncludingIgnored(),
            GetAXObjectByElementId("ignored_a"));
  EXPECT_EQ(GetAXObjectByElementId("ignored_a"),
            input->NextSiblingIncludingIgnored());
  ASSERT_NE(nullptr, br->NextSiblingIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            br->NextSiblingIncludingIgnored()->RoleValue());
  ASSERT_NE(nullptr, br->PreviousSiblingIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            br->PreviousSiblingIncludingIgnored()->RoleValue());

  EXPECT_EQ(paragraph->UnignoredPreviousSibling(), input);
  EXPECT_EQ(paragraph, input->UnignoredNextSibling());
  ASSERT_NE(nullptr, br->UnignoredNextSibling());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            br->UnignoredNextSibling()->RoleValue());
  ASSERT_NE(nullptr, br->UnignoredPreviousSibling());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            br->UnignoredPreviousSibling()->RoleValue());

  ASSERT_NE(nullptr, button->FirstChildIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            button->FirstChildIncludingIgnored()->RoleValue());
  ASSERT_NE(nullptr, button->LastChildIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            button->LastChildIncludingIgnored()->RoleValue());
  ASSERT_NE(nullptr, button->DeepestFirstChildIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            paragraph->DeepestFirstChildIncludingIgnored()->RoleValue());
}

TEST_F(AccessibilityTest, TreeNavigationWithIgnoredContainer) {
  // Setup the following tree :
  // ++A
  // ++IGNORED
  // ++++B
  // ++C
  // So that nodes [A, B, C] are siblings
  SetBodyInnerHTML(R"HTML(<body>
    <p id="A">some text</p>
    <div>
      <p id="B">nested text</p>
    </div>
    <p id="C">more text</p>
    </body>)HTML");

  const AXObject* root = GetAXRootObject();
  const AXObject* body = GetAXBodyObject();
  ASSERT_EQ(3, body->ChildCountIncludingIgnored());
  ASSERT_EQ(1, body->ChildAtIncludingIgnored(1)->ChildCountIncludingIgnored());

  ASSERT_FALSE(root->AccessibilityIsIgnored());
  ASSERT_TRUE(body->AccessibilityIsIgnored());
  const AXObject* obj_a = GetAXObjectByElementId("A");
  ASSERT_NE(nullptr, obj_a);
  ASSERT_FALSE(obj_a->AccessibilityIsIgnored());
  const AXObject* obj_a_text = obj_a->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, obj_a_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, obj_a_text->RoleValue());
  const AXObject* obj_b = GetAXObjectByElementId("B");
  ASSERT_NE(nullptr, obj_b);
  ASSERT_FALSE(obj_b->AccessibilityIsIgnored());
  const AXObject* obj_b_text = obj_b->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, obj_b_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, obj_b_text->RoleValue());
  const AXObject* obj_c = GetAXObjectByElementId("C");
  ASSERT_NE(nullptr, obj_c);
  ASSERT_FALSE(obj_c->AccessibilityIsIgnored());
  const AXObject* obj_c_text = obj_c->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, obj_c_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, obj_c_text->RoleValue());
  const AXObject* obj_ignored = body->ChildAtIncludingIgnored(1);
  ASSERT_NE(nullptr, obj_ignored);
  ASSERT_TRUE(obj_ignored->AccessibilityIsIgnored());

  EXPECT_EQ(root, obj_a->ParentObjectUnignored());
  EXPECT_EQ(body, obj_a->ParentObjectIncludedInTree());
  EXPECT_EQ(root, obj_b->ParentObjectUnignored());
  EXPECT_EQ(obj_ignored, obj_b->ParentObjectIncludedInTree());
  EXPECT_EQ(root, obj_c->ParentObjectUnignored());
  EXPECT_EQ(body, obj_c->ParentObjectIncludedInTree());

  EXPECT_EQ(obj_b, obj_ignored->FirstChildIncludingIgnored());

  EXPECT_EQ(nullptr, obj_a->PreviousSiblingIncludingIgnored());
  EXPECT_EQ(nullptr, obj_a->UnignoredPreviousSibling());
  EXPECT_EQ(obj_ignored, obj_a->NextSiblingIncludingIgnored());
  EXPECT_EQ(obj_b, obj_a->UnignoredNextSibling());

  EXPECT_EQ(body, obj_a->PreviousInPreOrderIncludingIgnored());
  EXPECT_EQ(root, obj_a->UnignoredPreviousInPreOrder());
  EXPECT_EQ(obj_a_text, obj_a->NextInPreOrderIncludingIgnored());
  EXPECT_EQ(obj_a_text, obj_a->UnignoredNextInPreOrder());

  EXPECT_EQ(nullptr, obj_b->PreviousSiblingIncludingIgnored());
  EXPECT_EQ(obj_a, obj_b->UnignoredPreviousSibling());
  EXPECT_EQ(nullptr, obj_b->NextSiblingIncludingIgnored());
  EXPECT_EQ(obj_c, obj_b->UnignoredNextSibling());

  EXPECT_EQ(obj_ignored, obj_b->PreviousInPreOrderIncludingIgnored());
  EXPECT_EQ(obj_a_text, obj_b->UnignoredPreviousInPreOrder());
  EXPECT_EQ(obj_b_text, obj_b->NextInPreOrderIncludingIgnored());
  EXPECT_EQ(obj_b_text, obj_b->UnignoredNextInPreOrder());

  EXPECT_EQ(obj_ignored, obj_c->PreviousSiblingIncludingIgnored());
  EXPECT_EQ(obj_b, obj_c->UnignoredPreviousSibling());
  EXPECT_EQ(nullptr, obj_c->NextSiblingIncludingIgnored());
  EXPECT_EQ(nullptr, obj_c->UnignoredNextSibling());

  EXPECT_EQ(obj_b_text, obj_c->PreviousInPreOrderIncludingIgnored());
  EXPECT_EQ(obj_b_text, obj_c->UnignoredPreviousInPreOrder());
  EXPECT_EQ(obj_c_text, obj_c->NextInPreOrderIncludingIgnored());
  EXPECT_EQ(obj_c_text, obj_c->UnignoredNextInPreOrder());
}

TEST_F(AccessibilityTest, AXObjectComparisonOperators) {
  SetBodyInnerHTML(R"HTML(<input id="input" type="text" value="value">
                   <p id="paragraph">hello<br id="br">there</p>
                   <button id="button">button</button>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, input);
  const AXObject* paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, paragraph);
  const AXObject* br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, br);
  const AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);

  EXPECT_TRUE(*root == *root);
  EXPECT_FALSE(*root != *root);
  EXPECT_FALSE(*root < *root);
  EXPECT_TRUE(*root <= *root);
  EXPECT_FALSE(*root > *root);
  EXPECT_TRUE(*root >= *root);

  EXPECT_TRUE(*input > *root);
  EXPECT_TRUE(*input >= *root);
  EXPECT_FALSE(*input < *root);
  EXPECT_FALSE(*input <= *root);

  EXPECT_TRUE(*input != *root);
  EXPECT_TRUE(*input < *paragraph);
  EXPECT_TRUE(*br > *input);
  EXPECT_TRUE(*paragraph < *br);
  EXPECT_TRUE(*br >= *paragraph);

  EXPECT_TRUE(*paragraph < *button);
  EXPECT_TRUE(*button > *br);
  EXPECT_FALSE(*button < *button);
  EXPECT_TRUE(*button <= *button);
  EXPECT_TRUE(*button >= *button);
  EXPECT_FALSE(*button > *button);
}

TEST_F(AccessibilityTest, AXObjectUnignoredAncestorsIterator) {
  SetBodyInnerHTML(
      R"HTML(<p id="paragraph"><b id="bold"><br id="br"></b></p>)HTML");

  AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  AXObject* paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, paragraph);
  AXObject* bold = GetAXObjectByElementId("bold");
  ASSERT_NE(nullptr, bold);
  AXObject* br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, br);
  ASSERT_EQ(ax::mojom::Role::kLineBreak, br->RoleValue());

  AXObject::AncestorsIterator iter = br->UnignoredAncestorsBegin();
  EXPECT_EQ(*paragraph, *iter);
  EXPECT_EQ(ax::mojom::Role::kParagraph, iter->RoleValue());
  EXPECT_EQ(*root, *++iter);
  EXPECT_EQ(*root, *iter++);
  EXPECT_EQ(br->UnignoredAncestorsEnd(), ++iter);
}

TEST_F(AccessibilityTest, AXObjectInOrderTraversalIterator) {
  SetBodyInnerHTML(R"HTML(<input type="checkbox" id="checkbox">)HTML");

  AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  AXObject* body = GetAXBodyObject();
  ASSERT_NE(nullptr, root);
  AXObject* checkbox = GetAXObjectByElementId("checkbox");
  ASSERT_NE(nullptr, checkbox);

  AXObject::InOrderTraversalIterator iter = body->GetInOrderTraversalIterator();
  EXPECT_EQ(*body, *iter);
  EXPECT_NE(GetAXObjectCache().InOrderTraversalEnd(), iter);
  EXPECT_EQ(*checkbox, *++iter);
  EXPECT_EQ(ax::mojom::Role::kCheckBox, iter->RoleValue());
  EXPECT_EQ(*checkbox, *iter++);
  EXPECT_EQ(GetAXObjectCache().InOrderTraversalEnd(), iter);
  EXPECT_EQ(*checkbox, *--iter);
  EXPECT_EQ(*checkbox, *iter--);
  --iter;  // Skip the BODY element.
  --iter;  // Skip the HTML element.
  EXPECT_EQ(ax::mojom::Role::kRootWebArea, iter->RoleValue());
  EXPECT_EQ(GetAXObjectCache().InOrderTraversalBegin(), iter);
}

TEST_F(AccessibilityTest, AxNodeObjectContainsHtmlAnchorElementUrl) {
  SetBodyInnerHTML(R"HTML(<a id="anchor" href="http://test.com">link</a>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* anchor = GetAXObjectByElementId("anchor");
  ASSERT_NE(nullptr, anchor);

  // Passing a malformed string to KURL returns an empty URL, so verify the
  // AXObject's URL is non-empty first to catch errors in the test itself.
  EXPECT_FALSE(anchor->Url().IsEmpty());
  EXPECT_EQ(anchor->Url(), KURL("http://test.com"));
}

TEST_F(AccessibilityTest, AxNodeObjectContainsSvgAnchorElementUrl) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <a id="anchor" xlink:href="http://test.com"></a>
    </svg>
  )HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* anchor = GetAXObjectByElementId("anchor");
  ASSERT_NE(nullptr, anchor);

  EXPECT_FALSE(anchor->Url().IsEmpty());
  EXPECT_EQ(anchor->Url(), KURL("http://test.com"));
}

TEST_F(AccessibilityTest, AxNodeObjectContainsImageUrl) {
  SetBodyInnerHTML(R"HTML(<img id="anchor" src="http://test.png" />)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* anchor = GetAXObjectByElementId("anchor");
  ASSERT_NE(nullptr, anchor);

  EXPECT_FALSE(anchor->Url().IsEmpty());
  EXPECT_EQ(anchor->Url(), KURL("http://test.png"));
}

TEST_F(AccessibilityTest, AxNodeObjectContainsInPageLinkTarget) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(<a id="anchor" href="#target">link</a>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* anchor = GetAXObjectByElementId("anchor");
  ASSERT_NE(nullptr, anchor);

  EXPECT_FALSE(anchor->Url().IsEmpty());
  EXPECT_EQ(anchor->Url(), KURL("http://test.com/#target"));
}

TEST_P(AccessibilityLayoutTest, NextOnLine) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html {
      font-size: 10px;
    }
    /* TODO(kojii): |NextOnLine| doesn't work for culled-inline.
       Ensure spans are not culled to avoid hitting the case. */
    span {
      background: gray;
    }
    </style>
    <div><span id="span1">a</span><span>b</span></div>
  )HTML");
  const AXObject* span1 = GetAXObjectByElementId("span1");
  ASSERT_NE(nullptr, span1);

  const AXObject* next = span1->NextOnLine();
  ASSERT_NE(nullptr, next);
  EXPECT_EQ("b", next->GetNode()->textContent());
}

TEST_F(AccessibilityTest, AxObjectPreservedWhitespaceIsLineBreakingObjects) {
  SetBodyInnerHTML(R"HTML(
    <span style="white-space: pre-line" id="preserved">
      First Paragraph
      Second Paragraph
      Third Paragraph
    </span>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);

  const AXObject* preserved_span = GetAXObjectByElementId("preserved");
  ASSERT_NE(nullptr, preserved_span);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, preserved_span->RoleValue());
  ASSERT_EQ(1, preserved_span->ChildCountIncludingIgnored());
  EXPECT_FALSE(preserved_span->IsLineBreakingObject());

  AXObject* preserved_text = preserved_span->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, preserved_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, preserved_text->RoleValue());
  EXPECT_FALSE(preserved_text->IsLineBreakingObject());

  // Expect 7 kInlineTextBox children
  // 3 lines of text, and 4 newlines
  preserved_text->LoadInlineTextBoxes();
  ASSERT_EQ(7, preserved_text->ChildCountIncludingIgnored());
  bool all_children_are_inline_text_boxes = true;
  for (const AXObject* child : preserved_text->ChildrenIncludingIgnored()) {
    if (child->RoleValue() != ax::mojom::Role::kInlineTextBox) {
      all_children_are_inline_text_boxes = false;
      break;
    }
  }
  ASSERT_TRUE(all_children_are_inline_text_boxes);

  ASSERT_EQ(preserved_text->ChildAtIncludingIgnored(0)->ComputedName(), "\n");
  EXPECT_TRUE(
      preserved_text->ChildAtIncludingIgnored(0)->IsLineBreakingObject());
  ASSERT_EQ(preserved_text->ChildAtIncludingIgnored(1)->ComputedName(),
            "First Paragraph");
  EXPECT_FALSE(
      preserved_text->ChildAtIncludingIgnored(1)->IsLineBreakingObject());
  ASSERT_EQ(preserved_text->ChildAtIncludingIgnored(2)->ComputedName(), "\n");
  EXPECT_TRUE(
      preserved_text->ChildAtIncludingIgnored(2)->IsLineBreakingObject());
  ASSERT_EQ(preserved_text->ChildAtIncludingIgnored(3)->ComputedName(),
            "Second Paragraph");
  EXPECT_FALSE(
      preserved_text->ChildAtIncludingIgnored(3)->IsLineBreakingObject());
  ASSERT_EQ(preserved_text->ChildAtIncludingIgnored(4)->ComputedName(), "\n");
  EXPECT_TRUE(
      preserved_text->ChildAtIncludingIgnored(4)->IsLineBreakingObject());
  ASSERT_EQ(preserved_text->ChildAtIncludingIgnored(5)->ComputedName(),
            "Third Paragraph");
  EXPECT_FALSE(
      preserved_text->ChildAtIncludingIgnored(5)->IsLineBreakingObject());
  ASSERT_EQ(preserved_text->ChildAtIncludingIgnored(6)->ComputedName(), "\n");
  EXPECT_TRUE(
      preserved_text->ChildAtIncludingIgnored(6)->IsLineBreakingObject());
}

TEST_F(AccessibilityTest, CheckNoDuplicateChildren) {
  GetPage().GetSettings().SetInlineTextBoxAccessibilityEnabled(false);
  SetBodyInnerHTML(R"HTML(
     <select id="sel"><option>1</option></select>
    )HTML");

  AXObject* ax_select = GetAXObjectByElementId("sel");
  ax_select->SetNeedsToUpdateChildren();
  ax_select->UpdateChildrenIfNecessary();

  ASSERT_EQ(
      ax_select->FirstChildIncludingIgnored()->ChildCountIncludingIgnored(), 1);
}

TEST_F(AccessibilityTest, InitRelationCache) {
  // All of the other tests already have accessibility initialized
  // first, but we don't want to in this test.
  //
  // Get rid of the AXContext so the AXObjectCache is destroyed.
  ax_context_.reset(nullptr);

  SetBodyInnerHTML(R"HTML(
      <ul id="ul" aria-owns="li"></ul>
      <label for="a"></label>
      <input id="a">
      <input id="b">
      <div role="section" id="div">
        <li id="li"></li>
      </div>
    )HTML");

  // Now recreate an AXContext, simulating what happens if accessibility
  // is enabled after the document is loaded.
  ax_context_.reset(new AXContext(GetDocument()));

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* input_a = GetAXObjectByElementId("a");
  ASSERT_NE(nullptr, input_a);
  const AXObject* input_b = GetAXObjectByElementId("b");
  ASSERT_NE(nullptr, input_b);

  EXPECT_TRUE(GetAXObjectCache().MayHaveHTMLLabel(
      To<HTMLElement>(*input_a->GetNode())));
  EXPECT_FALSE(GetAXObjectCache().MayHaveHTMLLabel(
      To<HTMLElement>(*input_b->GetNode())));

  // Note: retrieve the LI first and check that its parent is not
  // the paragraph element. If we were to retrieve the UL element,
  // that would trigger the aria-owns check and wouln't allow us to
  // test whether the relation cache was initialized.
  const AXObject* li = GetAXObjectByElementId("li");
  ASSERT_NE(nullptr, li);

  const AXObject* div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, div);
  EXPECT_NE(li->ParentObjectUnignored(), div);

  const AXObject* ul = GetAXObjectByElementId("ul");
  ASSERT_NE(nullptr, ul);

  EXPECT_EQ(li->ParentObjectUnignored(), ul);
}

}  // namespace test
}  // namespace blink
