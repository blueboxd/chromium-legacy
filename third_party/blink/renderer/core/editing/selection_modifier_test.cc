// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/selection_modifier.h"

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class SelectionModifierTest : public EditingTestBase {
 protected:
  std::string MoveBackwardByLine(SelectionModifier& modifier) {
    modifier.Modify(SelectionModifyAlteration::kMove,
                    SelectionModifyDirection::kBackward,
                    TextGranularity::kLine);
    return GetSelectionTextFromBody(modifier.Selection().AsSelection());
  }

  std::string MoveForwardByLine(SelectionModifier& modifier) {
    modifier.Modify(SelectionModifyAlteration::kMove,
                    SelectionModifyDirection::kForward, TextGranularity::kLine);
    return GetSelectionTextFromBody(modifier.Selection().AsSelection());
  }
};

TEST_F(SelectionModifierTest, ExtendForwardByWordNone) {
  SetBodyContent("abc");
  SelectionModifier modifier(GetFrame(), SelectionInDOMTree());
  modifier.Modify(SelectionModifyAlteration::kExtend,
                  SelectionModifyDirection::kForward, TextGranularity::kWord);
  // We should not crash here. See http://crbug.com/832061
  EXPECT_EQ(SelectionInDOMTree(), modifier.Selection().AsSelection());
}

TEST_F(SelectionModifierTest, MoveForwardByWordNone) {
  SetBodyContent("abc");
  SelectionModifier modifier(GetFrame(), SelectionInDOMTree());
  modifier.Modify(SelectionModifyAlteration::kMove,
                  SelectionModifyDirection::kForward, TextGranularity::kWord);
  // We should not crash here. See http://crbug.com/832061
  EXPECT_EQ(SelectionInDOMTree(), modifier.Selection().AsSelection());
}

TEST_F(SelectionModifierTest, MoveByLineHorizontal) {
  LoadAhem();
  InsertStyleElement(
      "p {"
      "font: 10px/20px Ahem;"
      "padding: 10px;"
      "writing-mode: horizontal-tb;"
      "}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<p>ab|c<br>d<br><br>ghi</p>");
  SelectionModifier modifier(GetFrame(), selection);

  EXPECT_EQ("<p>abc<br>d|<br><br>ghi</p>", MoveForwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d<br>|<br>ghi</p>", MoveForwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d<br><br>gh|i</p>", MoveForwardByLine(modifier));

  EXPECT_EQ("<p>abc<br>d<br>|<br>ghi</p>", MoveBackwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d|<br><br>ghi</p>", MoveBackwardByLine(modifier));
  EXPECT_EQ("<p>ab|c<br>d<br><br>ghi</p>", MoveBackwardByLine(modifier));
}

TEST_F(SelectionModifierTest, MoveByLineVertical) {
  LoadAhem();
  InsertStyleElement(
      "p {"
      "font: 10px/20px Ahem;"
      "padding: 10px;"
      "writing-mode: vertical-rl;"
      "}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<p>ab|c<br>d<br><br>ghi</p>");
  SelectionModifier modifier(GetFrame(), selection);

  EXPECT_EQ("<p>abc<br>d|<br><br>ghi</p>", MoveForwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d<br>|<br>ghi</p>", MoveForwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d<br><br>gh|i</p>", MoveForwardByLine(modifier));

  EXPECT_EQ("<p>abc<br>d<br>|<br>ghi</p>", MoveBackwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d|<br><br>ghi</p>", MoveBackwardByLine(modifier));
  EXPECT_EQ("<p>ab|c<br>d<br><br>ghi</p>", MoveBackwardByLine(modifier));
}

TEST_F(SelectionModifierTest, PreviousLineWithDisplayNone) {
  InsertStyleElement("body{font-family: monospace}");
  const SelectionInDOMTree selection = SetSelectionTextToBody(
      "<div contenteditable>"
      "<div>foo bar</div>"
      "<div>foo <b style=\"display:none\">qux</b> bar baz|</div>"
      "</div>");
  SelectionModifier modifier(GetFrame(), selection);
  modifier.Modify(SelectionModifyAlteration::kMove,
                  SelectionModifyDirection::kBackward, TextGranularity::kLine);
  EXPECT_EQ(
      "<div contenteditable>"
      "<div>foo bar|</div>"
      "<div>foo <b style=\"display:none\">qux</b> bar baz</div>"
      "</div>",
      GetSelectionTextFromBody(modifier.Selection().AsSelection()));
}

// For http://crbug.com/1104582
TEST_F(SelectionModifierTest, PreviousSentenceWithNull) {
  InsertStyleElement("b {display:inline-block}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<b><ruby><a>|</a></ruby></b>");
  SelectionModifier modifier(GetFrame(), selection);
  // We call |PreviousSentence()| with null-position.
  EXPECT_FALSE(modifier.Modify(SelectionModifyAlteration::kMove,
                               SelectionModifyDirection::kBackward,
                               TextGranularity::kSentence));
}

// For http://crbug.com/1100971
TEST_F(SelectionModifierTest, StartOfSentenceWithNull) {
  InsertStyleElement("b {display:inline-block}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("|<b><ruby><a></a></ruby></b>");
  SelectionModifier modifier(GetFrame(), selection);
  // We call |StartOfSentence()| with null-position.
  EXPECT_FALSE(modifier.Modify(SelectionModifyAlteration::kMove,
                               SelectionModifyDirection::kBackward,
                               TextGranularity::kSentenceBoundary));
}

}  // namespace blink
