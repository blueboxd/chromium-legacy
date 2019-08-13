// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_ax_action_target.h"

namespace pdf {

PdfAXActionTarget::PdfAXActionTarget(const ui::AXNode& plugin_node,
                                     pdf::PdfAccessibilityTree* pdf_tree_source)
    : target_plugin_node_(plugin_node),
      pdf_accessibility_tree_source_(pdf_tree_source) {
  DCHECK(pdf_accessibility_tree_source_);
}

PdfAXActionTarget::~PdfAXActionTarget() = default;

ui::AXActionTarget::Type PdfAXActionTarget::GetType() const {
  return ui::AXActionTarget::Type::kPdf;
}

bool PdfAXActionTarget::ClearAccessibilityFocus() const {
  return false;
}

bool PdfAXActionTarget::Click() const {
  return false;
}

bool PdfAXActionTarget::Decrement() const {
  return false;
}

bool PdfAXActionTarget::Increment() const {
  return false;
}

bool PdfAXActionTarget::Focus() const {
  return false;
}

gfx::Rect PdfAXActionTarget::GetRelativeBounds() const {
  return gfx::Rect();
}

gfx::Point PdfAXActionTarget::GetScrollOffset() const {
  return gfx::Point();
}

gfx::Point PdfAXActionTarget::MinimumScrollOffset() const {
  return gfx::Point();
}

gfx::Point PdfAXActionTarget::MaximumScrollOffset() const {
  return gfx::Point();
}

bool PdfAXActionTarget::SetAccessibilityFocus() const {
  return false;
}

void PdfAXActionTarget::SetScrollOffset(const gfx::Point& point) const {}

bool PdfAXActionTarget::SetSelected(bool selected) const {
  return false;
}

bool PdfAXActionTarget::SetSelection(const ui::AXActionTarget* anchor_object,
                                     int anchor_offset,
                                     const ui::AXActionTarget* focus_object,
                                     int focus_offset) const {
  return false;
}

bool PdfAXActionTarget::SetSequentialFocusNavigationStartingPoint() const {
  return false;
}

bool PdfAXActionTarget::SetValue(const std::string& value) const {
  return false;
}

bool PdfAXActionTarget::ShowContextMenu() const {
  return false;
}

bool PdfAXActionTarget::ScrollToMakeVisible() const {
  return false;
}

bool PdfAXActionTarget::ScrollToMakeVisibleWithSubFocus(
    const gfx::Rect& rect,
    ax::mojom::ScrollAlignment horizontal_scroll_alignment,
    ax::mojom::ScrollAlignment vertical_scroll_alignment) const {
  PP_PdfAccessibilityActionData pdf_action_data;
  pdf_action_data.action =
      PP_PdfAccessibilityAction::PP_PDF_SCROLL_TO_MAKE_VISIBLE;
  pdf_action_data.horizontal_scroll_alignment =
      ConvertAXScrollToPdfScrollAlignment(horizontal_scroll_alignment);
  pdf_action_data.vertical_scroll_alignment =
      ConvertAXScrollToPdfScrollAlignment(vertical_scroll_alignment);
  pdf_action_data.target_rect = {
      {target_plugin_node_.data().relative_bounds.bounds.x(),
       target_plugin_node_.data().relative_bounds.bounds.y()},
      {target_plugin_node_.data().relative_bounds.bounds.width(),
       target_plugin_node_.data().relative_bounds.bounds.height()}};
  pdf_accessibility_tree_source_->HandleAction(pdf_action_data);
  return true;
}

bool PdfAXActionTarget::ScrollToGlobalPoint(const gfx::Point& point) const {
  return false;
}

PP_PdfAccessibilityScrollAlignment
PdfAXActionTarget::ConvertAXScrollToPdfScrollAlignment(
    ax::mojom::ScrollAlignment scroll_alignment) const {
  switch (scroll_alignment) {
    case ax::mojom::ScrollAlignment::kScrollAlignmentCenter:
      return PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_CENTER;
    case ax::mojom::ScrollAlignment::kScrollAlignmentTop:
      return PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_TOP;
    case ax::mojom::ScrollAlignment::kScrollAlignmentBottom:
      return PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_BOTTOM;
    case ax::mojom::ScrollAlignment::kScrollAlignmentLeft:
      return PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_LEFT;
    case ax::mojom::ScrollAlignment::kScrollAlignmentRight:
      return PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_RIGHT;
    case ax::mojom::ScrollAlignment::kScrollAlignmentClosestEdge:
      return PP_PdfAccessibilityScrollAlignment::
          PP_PDF_SCROLL_ALIGNMENT_CLOSEST_EDGE;
    case ax::mojom::ScrollAlignment::kNone:
    default:
      return PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_NONE;
  }
}

}  // namespace pdf
