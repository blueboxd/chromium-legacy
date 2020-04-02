// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_
#define UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_

#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"

namespace ui {

// This file contains various helper functions that determine whether a
// specific accessibility role meets certain criteria.
//
// Please keep these functions in alphabetic order.

// Returns true for objects which have the characteristic "Children
// Presentational: True". This concept is defined in the ARIA specification. See
// https://www.w3.org/TR/wai-aria-1.1/#childrenArePresentational.
AX_BASE_EXPORT bool HasPresentationalChildren(const ax::mojom::Role role);

// Checks if the given role is an alert or alert-dialog type.
AX_BASE_EXPORT bool IsAlert(const ax::mojom::Role role);

// Checks if the given role is a clickable type.
AX_BASE_EXPORT bool IsClickable(const ax::mojom::Role role);

// Returns true if the provided role belongs to a cell or a table header.
AX_BASE_EXPORT bool IsCellOrTableHeader(const ax::mojom::Role role);

// Returns true if the provided role belongs to a container with selectable
// children.
AX_BASE_EXPORT bool IsContainerWithSelectableChildren(
    const ax::mojom::Role role);

// Returns true if the provided role is a control.
AX_BASE_EXPORT bool IsControl(const ax::mojom::Role role);

// Returns true if the provided role is a control on the Android platform.
AX_BASE_EXPORT bool IsControlOnAndroid(const ax::mojom::Role role,
                                       bool isFocusable);

// Returns true if the provided role belongs to a document.
AX_BASE_EXPORT bool IsDocument(const ax::mojom::Role role);

// Returns true if the provided role represents a dialog.
AX_BASE_EXPORT bool IsDialog(const ax::mojom::Role role);

// Returns true if the provided role is a form.
AX_BASE_EXPORT bool IsForm(const ax::mojom::Role role);

// Returns true if crossing into or out of the provided role should count as
// crossing a format boundary.
AX_BASE_EXPORT bool IsFormatBoundary(const ax::mojom::Role role);

// Returns true if the provided role belongs to a heading.
AX_BASE_EXPORT bool IsHeading(const ax::mojom::Role role);

// Returns true if the provided role belongs to a heading or a table header.
AX_BASE_EXPORT bool IsHeadingOrTableHeader(const ax::mojom::Role role);

// Returns true if the provided role belongs to an iframe.
AX_BASE_EXPORT bool IsIframe(const ax::mojom::Role role);

// Returns true if the provided role is for any kind of image or video.
AX_BASE_EXPORT bool IsImageOrVideo(const ax::mojom::Role role);

// Returns true if the provided role belongs to an image, graphic, canvas, etc.
AX_BASE_EXPORT bool IsImage(const ax::mojom::Role role);

// Returns true if the provided role is item-like, specifically if it can hold
// pos_in_set and set_size values.
AX_BASE_EXPORT bool IsItemLike(const ax::mojom::Role role);

// Returns true if the role is a subclass of the ARIA Landmark abstract role.
AX_BASE_EXPORT bool IsLandmark(const ax::mojom::Role role);

// Returns true if the provided role belongs to a link.
AX_BASE_EXPORT bool IsLink(const ax::mojom::Role role);

// Returns true if the provided role belongs to a list.
AX_BASE_EXPORT bool IsList(const ax::mojom::Role role);

// Returns true if the provided role belongs to a list item.
AX_BASE_EXPORT bool IsListItem(const ax::mojom::Role role);

// Returns true if the provided role belongs to a menu item, including menu item
// checkbox and menu item radio buttons.
AX_BASE_EXPORT bool IsMenuItem(ax::mojom::Role role);

// Returns true if the provided role belongs to a menu or related control.
AX_BASE_EXPORT bool IsMenuRelated(const ax::mojom::Role role);

// Returns true if this object supports readonly.
// Note: This returns false for table cells and headers, it is up to the
//       caller to make sure that they are included IFF they are within an
//       ARIA-1.1+ role='grid' or 'treegrid', and not role='table'.
AX_BASE_EXPORT bool IsReadOnlySupported(const ax::mojom::Role role);

// Returns true if the provided role belongs to a widget that can contain a
// table or grid row.
AX_BASE_EXPORT bool IsRowContainer(const ax::mojom::Role role);

// Returns true if the role is a subclass of the ARIA Section abstract role.
AX_BASE_EXPORT bool IsSection(const ax::mojom::Role role);

// Returns true if the role is a subclass of the ARIA Sectionhead role.
AX_BASE_EXPORT bool IsSectionhead(const ax::mojom::Role role);

// Returns true if the role is a subclass of the ARIA Select abstract role.
AX_BASE_EXPORT bool IsSelect(const ax::mojom::Role role);

// Returns true if the provided role is ordered-set like, specifically if it
// can hold set_size values.
AX_BASE_EXPORT bool IsSetLike(const ax::mojom::Role role);

// Returns true if the provided role belongs to a non-interactive list.
AX_BASE_EXPORT bool IsStaticList(const ax::mojom::Role role);

// Returns true if the role is a subclass of the ARIA Structure abstract role.
AX_BASE_EXPORT bool IsStructure(const ax::mojom::Role role);

// Returns true if the provided role belongs to a table or grid column, and the
// table is not used for layout purposes.
AX_BASE_EXPORT bool IsTableColumn(ax::mojom::Role role);

// Returns true if the provided role belongs to a table header.
AX_BASE_EXPORT bool IsTableHeader(ax::mojom::Role role);

// Returns true if the provided role belongs to a table, a grid or a treegrid.
AX_BASE_EXPORT bool IsTableLike(const ax::mojom::Role role);

// Returns true if the provided role belongs to a table or grid row, and the
// table is not used for layout purposes.
AX_BASE_EXPORT bool IsTableRow(ax::mojom::Role role);

// Returns true if it's a text-related node e.g. static text, line break, or
// inline text box node.
AX_BASE_EXPORT bool IsText(ax::mojom::Role role);

// Returns true if it's a text-related node e.g. a static text or line break
// node.
AX_BASE_EXPORT bool IsTextOrLineBreak(ax::mojom::Role role);

// Returns true if the role supports expand/collapse.
AX_BASE_EXPORT bool SupportsExpandCollapse(const ax::mojom::Role role);

// Returns true if the role supports hierarchical level.
AX_BASE_EXPORT bool SupportsHierarchicalLevel(const ax::mojom::Role role);

// Returns true if the provided role can have an orientation.
AX_BASE_EXPORT bool SupportsOrientation(const ax::mojom::Role role);

// Returns true if the provided role supports toggle.
AX_BASE_EXPORT bool SupportsToggle(const ax::mojom::Role role);

// Returns true if the node should be read only by default
AX_BASE_EXPORT bool ShouldHaveReadonlyStateByDefault(
    const ax::mojom::Role role);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_
