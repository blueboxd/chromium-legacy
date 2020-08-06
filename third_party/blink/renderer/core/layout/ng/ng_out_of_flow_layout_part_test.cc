// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {
namespace {

class NGOutOfFlowLayoutPartTest
    : public NGBaseLayoutAlgorithmTest,
      private ScopedLayoutNGBlockFragmentationForTest {
 protected:
  NGOutOfFlowLayoutPartTest() : ScopedLayoutNGBlockFragmentationForTest(true) {}

  scoped_refptr<const NGPhysicalBoxFragment> RunBlockLayoutAlgorithm(
      Element* element) {
    NGBlockNode container(ToLayoutBox(element->GetLayoutObject()));
    NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
        WritingMode::kHorizontalTb, TextDirection::kLtr,
        LogicalSize(LayoutUnit(1000), kIndefiniteSize));
    return NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(container, space);
  }

  String DumpFragmentTree(Element* element) {
    auto fragment = RunBlockLayoutAlgorithm(element);
    return DumpFragmentTree(fragment.get());
  }

  String DumpFragmentTree(const blink::NGPhysicalBoxFragment* fragment) {
    NGPhysicalFragment::DumpFlags flags =
        NGPhysicalFragment::DumpHeaderText | NGPhysicalFragment::DumpSubtree |
        NGPhysicalFragment::DumpIndentation | NGPhysicalFragment::DumpOffset |
        NGPhysicalFragment::DumpSize;

    return fragment->DumpFragmentTree(flags);
  }
};

// Fixed blocks inside absolute blocks trigger otherwise unused while loop
// inside NGOutOfFlowLayoutPart::Run.
// This test exercises this loop by placing two fixed elements inside abs.
TEST_F(NGOutOfFlowLayoutPartTest, FixedInsideAbs) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        body{ padding:0px; margin:0px}
        #rel { position:relative }
        #abs {
          position: absolute;
          top:49px;
          left:0px;
        }
        #pad {
          width:100px;
          height:50px;
        }
        #fixed1 {
          position:fixed;
          width:50px;
        }
        #fixed2 {
          position:fixed;
          top:9px;
          left:7px;
        }
      </style>
      <div id='rel'>
        <div id='abs'>
          <div id='pad'></div>
          <div id='fixed1'>
            <p>fixed static</p>
          </div>
          <div id='fixed2'>
            <p>fixed plain</p>
          </div>
        </div>
      </div>
      )HTML");

  // Test whether the oof fragments have been collected at NG->Legacy boundary.
  Element* rel = GetDocument().getElementById("rel");
  auto* block_flow = To<LayoutBlockFlow>(rel->GetLayoutObject());
  scoped_refptr<const NGLayoutResult> result =
      block_flow->GetCachedLayoutResult();
  EXPECT_TRUE(result);
  EXPECT_EQ(result->PhysicalFragment().OutOfFlowPositionedDescendants().size(),
            2u);

  // Test the final result.
  Element* fixed_1 = GetDocument().getElementById("fixed1");
  Element* fixed_2 = GetDocument().getElementById("fixed2");
  // fixed1 top is static: #abs.top + #pad.height
  EXPECT_EQ(fixed_1->OffsetTop(), LayoutUnit(99));
  // fixed2 top is positioned: #fixed2.top
  EXPECT_EQ(fixed_2->OffsetTop(), LayoutUnit(9));
}

// Tests non-fragmented positioned nodes inside a multi-column.
TEST_F(NGOutOfFlowLayoutPartTest, PositionedInMulticol) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count: 2; height: 40px; column-fill: auto; column-gap: 16px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position: absolute;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div style="width:100px; height:50px;"></div>
          <div class="rel" style="width:30px;">
            <div class="abs" style="width:5px; top:10px; height:5px;">
            </div>
            <div class="rel" style="width:35px; padding-top:8px;">
              <div class="abs" style="width:10px; top:20px; height:10px;">
              </div>
            </div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:100x40
        offset:0,20 size:10x10
        offset:0,10 size:5x5
      offset:508,0 size:492x40
        offset:0,0 size:100x10
        offset:0,10 size:30x8
          offset:0,0 size:35x8
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that positioned nodes fragment correctly.
TEST_F(NGOutOfFlowLayoutPartTest, SimplePositionedFragmentation) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; top:0px; width:5px; height:50px;
          border:solid 2px; margin-top:5px; padding:5px;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div style="width:100px; height:50px;"></div>
          <div class="rel">
            <div class="abs"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  // TODO(bebeaudr): The OOF positioned element should start in the second
  // column rather than the first.
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:100x40
        offset:0,5 size:19x35
      offset:508,0 size:492x40
        offset:0,0 size:100x10
        offset:0,10 size:30x0
        offset:0,0 size:19x29
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests fragmentation when a positioned node's child overflows.
TEST_F(NGOutOfFlowLayoutPartTest, PositionedFragmentationWithOverflow) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; top:10px; width:5px; height:10px;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel">
            <div class="abs">
              <div style="width:100px; height:50px;"></div>
            </div>
          </div>
          <div style="width:20px; height:100px;"></div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:30x0
        offset:0,0 size:20x40
        offset:0,10 size:5x10
          offset:0,0 size:100x30
      offset:508,0 size:492x40
        offset:0,0 size:20x40
        offset:0,0 size:5x0
          offset:0,0 size:100x20
      offset:1016,0 size:492x40
        offset:0,0 size:20x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that new column fragments are added correctly if a positioned node
// fragments beyond the last fragmentainer in a context.
TEST_F(NGOutOfFlowLayoutPartTest, PositionedFragmentationWithNewColumns) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; width:5px; height:120px;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel">
            <div class="abs"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:30x0
        offset:0,0 size:5x40
      offset:508,0 size:492x40
        offset:0,0 size:5x40
      offset:1016,0 size:492x40
        offset:0,0 size:5x40
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that empty column fragments are added if an OOF element begins layout
// in a fragmentainer that is more than one index beyond the last existing
// column fragmentainer.
TEST_F(NGOutOfFlowLayoutPartTest, PositionedFragmentationWithNewEmptyColumns) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; top:80px; width:5px; height:120px;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel">
            <div class="abs"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  // TODO(bebeaudr): The OOF fragment should start at a column fragment with
  // index 2, and there should be an empty column fragment at index 1.
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:30x0
        offset:0,80 size:5x120
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Break-inside does not apply to absolute positioned elements.
TEST_F(NGOutOfFlowLayoutPartTest, BreakInsideAvoid) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position:absolute; break-inside:avoid;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div style="width:20px; height:10px;"></div>
          <div class="rel" style="width:30px;">
            <div class="abs" style="width:40px; height:40px;"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  // TODO(bebeaudr): The abspos should have an offset of (0,10), and
  // break into a second column, ignoring "break-inside:avoid".
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:20x10
        offset:0,10 size:30x0
        offset:0,0 size:40x40
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Break-before does not apply to absolute positioned elements.
TEST_F(NGOutOfFlowLayoutPartTest, BreakBeforeColumn) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position:absolute; break-before:column;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div style="width:10px; height:30px;"></div>
          <div class="rel" style="width:30px;">
            <div class="abs" style="width:40px; height:30px;"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  // TODO(bebeaudr): The abspos should have an offset of (0,30) in the first
  // column, ignoring "break-before:column", and break into a second column.
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:10x30
        offset:0,30 size:30x0
        offset:0,0 size:40x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Break-after does not apply to absolute positioned elements.
TEST_F(NGOutOfFlowLayoutPartTest, BreakAfterColumn) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position:absolute; break-after:column;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div style="width:10px; height:20px;"></div>
          <div class="rel" style="width:30px; height:10px;">
            <div class="abs" style="width:40px; height:10px;"></div>
          </div>
          <div style="width:20px; height:10px;"></div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  // TODO(bebeaudr): The abspos should have an offset of (0,20).
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:10x20
        offset:0,20 size:30x10
        offset:0,30 size:20x10
        offset:0,0 size:40x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Break-inside should still apply to children of absolute positioned elements.
TEST_F(NGOutOfFlowLayoutPartTest, ChildBreakInsideAvoid) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:100px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position:absolute;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel" style="width:30px;">
            <div class="abs" style="width:40px; height:150px;">
              <div style="width:15px; height:50px;"></div>
              <div style="break-inside:avoid; width:20px; height:100px;"></div>
            </div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:1000x100
      offset:0,0 size:492x100
        offset:0,0 size:30x0
        offset:0,0 size:40x100
          offset:0,0 size:15x50
      offset:508,0 size:492x100
        offset:0,0 size:40x50
          offset:0,0 size:20x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Break-before should still apply to children of absolute positioned elements.
TEST_F(NGOutOfFlowLayoutPartTest, ChildBreakBeforeAvoid) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:100px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position:absolute;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel" style="width:30px;">
            <div class="abs" style="width:40px; height:150px;">
              <div style="width:15px; height:50px;"></div>
              <div style="width:20px; height:50px;"></div>
              <div style="break-before:avoid; width:10px; height:20px;"></div>
            </div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:1000x100
      offset:0,0 size:492x100
        offset:0,0 size:30x0
        offset:0,0 size:40x100
          offset:0,0 size:15x50
      offset:508,0 size:492x100
        offset:0,0 size:40x50
          offset:0,0 size:20x50
          offset:0,50 size:10x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Break-after should still apply to children of absolute positioned elements.
TEST_F(NGOutOfFlowLayoutPartTest, ChildBreakAfterAvoid) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:100px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position:absolute;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel" style="width:30px;">
            <div class="abs" style="width:40px; height:150px;">
              <div style="width:15px; height:50px;"></div>
              <div style="break-after:avoid; width:20px; height:50px;"></div>
              <div style="width:10px; height:20px;"></div>
            </div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:1000x100
      offset:0,0 size:492x100
        offset:0,0 size:30x0
        offset:0,0 size:40x100
          offset:0,0 size:15x50
      offset:508,0 size:492x100
        offset:0,0 size:40x50
          offset:0,0 size:20x50
          offset:0,50 size:10x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

}  // namespace
}  // namespace blink
