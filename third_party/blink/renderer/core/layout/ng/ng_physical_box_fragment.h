// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalBoxFragment_h
#define NGPhysicalBoxFragment_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/platform/graphics/scroll_types.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class NGBoxFragmentBuilder;
enum class NGOutlineType;

class CORE_EXPORT NGPhysicalBoxFragment final
    : public NGPhysicalContainerFragment {
 public:
  static scoped_refptr<const NGPhysicalBoxFragment> Create(
      NGBoxFragmentBuilder* builder,
      WritingMode block_or_line_writing_mode);

  ~NGPhysicalBoxFragment() {
    for (const NGLink& child : Children())
      child.fragment->Release();
  }

  base::Optional<LayoutUnit> Baseline(const NGBaselineRequest& request) const {
    return baselines_.Offset(request);
  }

  const NGPhysicalBoxStrut Borders() const {
    if (!has_borders_)
      return NGPhysicalBoxStrut();
    return *ComputeBordersAddress();
  }

  const NGPhysicalBoxStrut Padding() const {
    if (!has_padding_)
      return NGPhysicalBoxStrut();
    return *ComputePaddingAddress();
  }

  NGPixelSnappedPhysicalBoxStrut PixelSnappedPadding() const {
    if (!has_padding_)
      return NGPixelSnappedPhysicalBoxStrut();
    return ComputePaddingAddress()->SnapToDevicePixels();
  }

  bool HasSelfPaintingLayer() const;
  bool ChildrenInline() const { return children_inline_; }

  PhysicalRect ScrollableOverflow() const;

  // TODO(layout-dev): These three methods delegate to legacy layout for now,
  // update them to use LayoutNG based overflow information from the fragment
  // and change them to use NG geometry types once LayoutNG supports overflow.
  PhysicalRect OverflowClipRect(
      const PhysicalOffset& location,
      OverlayScrollbarClipBehavior = kIgnorePlatformOverlayScrollbarSize) const;
  IntSize ScrolledContentOffset() const;
  PhysicalSize ScrollSize() const;

  // Compute visual overflow of this box in the local coordinate.
  PhysicalRect ComputeSelfInkOverflow() const;

  // Fragment offset is this fragment's offset from parent.
  // Needed to compensate for LayoutInline Legacy code offsets.
  void AddSelfOutlineRects(Vector<PhysicalRect>* outline_rects,
                           const PhysicalOffset& additional_offset,
                           NGOutlineType include_block_overflows) const;

  UBiDiLevel BidiLevel() const;

  // Bitmask for border edges, see NGBorderEdges::Physical.
  unsigned BorderEdges() const { return border_edge_; }
  NGPixelSnappedPhysicalBoxStrut BorderWidths() const;

#if DCHECK_IS_ON()
  void CheckSameForSimplifiedLayout(const NGPhysicalBoxFragment&,
                                    bool check_same_block_size) const;
#endif

 private:
  NGPhysicalBoxFragment(NGBoxFragmentBuilder* builder,
                        const NGPhysicalBoxStrut& borders,
                        const NGPhysicalBoxStrut& padding,
                        WritingMode block_or_line_writing_mode);

  const NGPhysicalBoxStrut* ComputeBordersAddress() const {
    DCHECK(has_borders_);
    return reinterpret_cast<const NGPhysicalBoxStrut*>(children_ +
                                                       Children().size());
  }

  const NGPhysicalBoxStrut* ComputePaddingAddress() const {
    DCHECK(has_padding_);
    const NGPhysicalBoxStrut* address =
        reinterpret_cast<const NGPhysicalBoxStrut*>(children_ +
                                                    Children().size());
    return has_borders_ ? address + 1 : address;
  }

  NGBaselineList baselines_;
  NGLink children_[];
  // borders and padding come from after |children_| if they are not zero.
};

template <>
struct DowncastTraits<NGPhysicalBoxFragment> {
  static bool AllowFrom(const NGPhysicalFragment& fragment) {
    return fragment.Type() == NGPhysicalFragment::kFragmentBox ||
           fragment.Type() == NGPhysicalFragment::kFragmentRenderedLegend;
  }
};

}  // namespace blink

#endif  // NGPhysicalBoxFragment_h
