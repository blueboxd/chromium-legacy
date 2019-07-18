// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_H_

#include <stdint.h>

#include <memory>
#include <new>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "ui/accessibility/ax_clipping_behavior.h"
#include "ui/accessibility/ax_coordinate_system.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_offscreen_result.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_text_boundary.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {

class Rect;
}

namespace ui {

struct AXActionData;
struct AXNodeData;
struct AXTreeData;
class AXPlatformNode;

// An object that wants to be accessible should derive from this class.
// AXPlatformNode subclasses use this interface to query all of the information
// about the object in order to implement native accessibility APIs.
//
// Note that AXPlatformNode has support for accessibility trees where some
// of the objects in the tree are not implemented using AXPlatformNode.
// For example, you may have a native window with platform-native widgets
// in it, but in that window you have custom controls that use AXPlatformNode
// to provide accessibility. That's why GetParent, ChildAtIndex, HitTestSync,
// and GetFocus all return a gfx::NativeViewAccessible - so you can return a
// native accessible if necessary, and AXPlatformNode::GetNativeViewAccessible
// otherwise.
class AX_EXPORT AXPlatformNodeDelegate {
 public:
  virtual ~AXPlatformNodeDelegate() = default;

  // Get the accessibility data that should be exposed for this node.
  // Virtually all of the information is obtained from this structure
  // (role, state, name, cursor position, etc.) - the rest of this interface
  // is mostly to implement support for walking the accessibility tree.
  virtual const AXNodeData& GetData() const = 0;

  // Get the accessibility tree data for this node.
  virtual const AXTreeData& GetTreeData() const = 0;

  // Creates a text position rooted at this object.
  virtual AXNodePosition::AXPositionInstance CreateTextPositionAt(
      int offset,
      ax::mojom::TextAffinity affinity =
          ax::mojom::TextAffinity::kDownstream) const = 0;

  // Get the accessibility node for the NSWindow the node is contained in. This
  // method is only meaningful on macOS.
  virtual gfx::NativeViewAccessible GetNSWindow() = 0;

  // Get the parent of the node, which may be an AXPlatformNode or it may
  // be a native accessible object implemented by another class.
  virtual gfx::NativeViewAccessible GetParent() = 0;

  // Get the index in parent. Typically this is the AXNode's index_in_parent_.
  virtual int GetIndexInParent() = 0;

  // Get the number of children of this node.
  virtual int GetChildCount() = 0;

  // Get the child of a node given a 0-based index.
  virtual gfx::NativeViewAccessible ChildAtIndex(int index) = 0;

  // Gets the first child of a node, or nullptr if no children exist.
  virtual gfx::NativeViewAccessible GetFirstChild() = 0;

  // Gets the last child of a node, or nullptr if no children exist.
  virtual gfx::NativeViewAccessible GetLastChild() = 0;

  // Gets the next sibling of a given node, or nullptr if no such node exists.
  virtual gfx::NativeViewAccessible GetNextSibling() = 0;

  // Gets the previous sibling of a given node, or nullptr if no such node
  // exists.
  virtual gfx::NativeViewAccessible GetPreviousSibling() = 0;

  class ChildIterator {
   public:
    virtual ~ChildIterator() {}
    virtual bool operator==(const ChildIterator& rhs) const = 0;
    virtual bool operator!=(const ChildIterator& rhs) const = 0;
    virtual void operator++() = 0;
    virtual void operator++(int) = 0;
    virtual void operator--() = 0;
    virtual void operator--(int) = 0;
    virtual gfx::NativeViewAccessible GetNativeViewAccessible() const = 0;
    virtual int GetIndexInParent() const = 0;
  };
  virtual std::unique_ptr<AXPlatformNodeDelegate::ChildIterator>
  ChildrenBegin() = 0;
  virtual std::unique_ptr<AXPlatformNodeDelegate::ChildIterator>
  ChildrenEnd() = 0;

  // Returns the text of this node and represent the text of descendant nodes
  // with a special character in place of every embedded object. This represents
  // the concept of text in ATK and IA2 APIs.
  virtual base::string16 GetHypertext() const = 0;

  // Set the selection in the hypertext of this node. Depending on the
  // implementation, this may mean the new selection will span multiple nodes.
  virtual bool SetHypertextSelection(int start_offset, int end_offset) = 0;

  // Returns the text of this node and all descendant nodes; including text
  // found in embedded objects.
  virtual base::string16 GetInnerText() const = 0;

  // Return the bounds of this node in the coordinate system indicated. If the
  // clipping behavior is set to clipped, clipping is applied. If an offscreen
  // result address is provided, it will be populated depending on whether the
  // returned bounding box is onscreen or offscreen.
  virtual gfx::Rect GetBoundsRect(
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result = nullptr) const = 0;

  // Return the bounds of the text range given by text offsets relative to
  // GetHypertext in the coordinate system indicated. If the clipping behavior
  // is set to clipped, clipping is applied. If an offscreen result address is
  // provided, it will be populated depending on whether the returned bounding
  // box is onscreen or offscreen.
  virtual gfx::Rect GetHypertextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result = nullptr) const = 0;

  // Return the bounds of the text range given by text offsets relative to
  // GetInnerText in the coordinate system indicated. If the clipping behavior
  // is set to clipped, clipping is applied. If an offscreen result address is
  // provided, it will be populated depending on whether the returned bounding
  // box is onscreen or offscreen.
  virtual gfx::Rect GetInnerTextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result = nullptr) const = 0;

  // Do a *synchronous* hit test of the given location in global screen
  // coordinates, and the node within this node's subtree (inclusive) that's
  // hit, if any.
  //
  // If the result is anything other than this object or NULL, it will be
  // hit tested again recursively - that allows hit testing to work across
  // implementation classes. It's okay to take advantage of this and return
  // only an immediate child and not the deepest descendant.
  //
  // This function is mainly used by accessibility debugging software.
  // Platforms with touch accessibility use a different asynchronous interface.
  virtual gfx::NativeViewAccessible HitTestSync(int x, int y) = 0;

  // Return the node within this node's subtree (inclusive) that currently has
  // focus, or return nullptr if this subtree is not connected to the top
  // document through its ancestry chain.
  virtual gfx::NativeViewAccessible GetFocus() = 0;

  // Get whether this node is offscreen.
  virtual bool IsOffscreen() const = 0;

  // Get whether this node is a minimized window.
  virtual bool IsMinimized() const = 0;

  // Get whether this node is in web content.
  virtual bool IsWebContent() const = 0;

  virtual AXPlatformNode* GetFromNodeID(int32_t id) = 0;

  // Given a node ID attribute (one where IsNodeIdIntAttribute is true), return
  // a target nodes for which this delegate's node has that relationship
  // attribute or NULL if there is no such relationship.
  virtual AXPlatformNode* GetTargetNodeForRelation(
      ax::mojom::IntAttribute attr) = 0;

  // Given a node ID attribute (one where IsNodeIdIntListAttribute is true),
  // return a set of all target nodes for which this delegate's node has that
  // relationship attribute.
  virtual std::set<AXPlatformNode*> GetTargetNodesForRelation(
      ax::mojom::IntListAttribute attr) = 0;

  // Given a node ID attribute (one where IsNodeIdIntAttribute is true), return
  // a set of all source nodes that have that relationship attribute between
  // them and this delegate's node.
  virtual std::set<AXPlatformNode*> GetReverseRelations(
      ax::mojom::IntAttribute attr) = 0;

  // Given a node ID list attribute (one where IsNodeIdIntListAttribute is
  // true), return a set of all source nodes that have that relationship
  // attribute between them and this delegate's node.
  virtual std::set<AXPlatformNode*> GetReverseRelations(
      ax::mojom::IntListAttribute attr) = 0;

  // Return the string representation of the unique ID assigned by the author,
  // otherwise return an empty string. The author ID must be persistent across
  // any instance of the application, regardless of locale. The author ID should
  // be unique among sibling accessibility nodes and is best if unique across
  // the application, however, not meeting this requirement is non-fatal.
  virtual base::string16 GetAuthorUniqueId() const = 0;

  virtual const AXUniqueId& GetUniqueId() const = 0;

  // This method finds text boundaries in the text used for platform text APIs.
  // Implementations may use side-channel data such as line or word indices to
  // produce appropriate results. It may optionally return no value, indicating
  // that the delegate does not have all the information required to calculate
  // this value and it is the responsibility of the AXPlatformNode itself to
  // to calculate it.
  virtual base::Optional<int> FindTextBoundary(
      AXTextBoundary boundary,
      int offset,
      TextBoundaryDirection direction,
      ax::mojom::TextAffinity affinity) const = 0;

  // Return a vector of all the descendants of this delegate's node.
  virtual const std::vector<gfx::NativeViewAccessible> GetDescendants()
      const = 0;

  // Return a string representing the language code.
  //
  // For web content, this will consider the language declared in the DOM, and
  // may eventually attempt to automatically detect the language from the text.
  //
  // This language code will be BCP 47.
  //
  // Returns empty string if no appropriate language was found or if this node
  // uses the default interface language.
  virtual std::string GetLanguage() const = 0;

  //
  // Tables. All of these should be called on a node that's a table-like
  // role, otherwise they return nullopt.
  //
  virtual bool IsTable() const = 0;
  virtual base::Optional<int> GetTableColCount() const = 0;
  virtual base::Optional<int> GetTableRowCount() const = 0;
  virtual base::Optional<int> GetTableAriaColCount() const = 0;
  virtual base::Optional<int> GetTableAriaRowCount() const = 0;
  virtual base::Optional<int> GetTableCellCount() const = 0;
  virtual std::vector<int32_t> GetColHeaderNodeIds() const = 0;
  virtual std::vector<int32_t> GetColHeaderNodeIds(int col_index) const = 0;
  virtual std::vector<int32_t> GetRowHeaderNodeIds() const = 0;
  virtual std::vector<int32_t> GetRowHeaderNodeIds(int row_index) const = 0;
  virtual AXPlatformNode* GetTableCaption() const = 0;

  // Table row-like nodes.
  virtual bool IsTableRow() const = 0;
  virtual base::Optional<int> GetTableRowRowIndex() const = 0;

  // Table cell-like nodes.
  virtual bool IsTableCellOrHeader() const = 0;
  virtual base::Optional<int> GetTableCellIndex() const = 0;
  virtual base::Optional<int> GetTableCellColIndex() const = 0;
  virtual base::Optional<int> GetTableCellRowIndex() const = 0;
  virtual base::Optional<int> GetTableCellColSpan() const = 0;
  virtual base::Optional<int> GetTableCellRowSpan() const = 0;
  virtual base::Optional<int> GetTableCellAriaColIndex() const = 0;
  virtual base::Optional<int> GetTableCellAriaRowIndex() const = 0;
  virtual base::Optional<int32_t> GetCellId(int row_index,
                                            int col_index) const = 0;
  virtual base::Optional<int32_t> CellIndexToId(int cell_index) const = 0;

  // Helper methods to check if a cell is an ARIA-1.1+ 'cell' or 'gridcell'
  virtual bool IsCellOrHeaderOfARIATable() const = 0;
  virtual bool IsCellOrHeaderOfARIAGrid() const = 0;

  // Ordered-set-like and item-like nodes.
  virtual bool IsOrderedSetItem() const = 0;
  virtual bool IsOrderedSet() const = 0;
  virtual base::Optional<int> GetPosInSet() const = 0;
  virtual base::Optional<int> GetSetSize() const = 0;

  //
  // Events.
  //

  // Return the platform-native GUI object that should be used as a target
  // for accessibility events.
  virtual gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() = 0;

  //
  // Actions.
  //

  // Perform an accessibility action, switching on the ax::mojom::Action
  // provided in |data|.
  virtual bool AccessibilityPerformAction(const AXActionData& data) = 0;

  //
  // Localized strings.
  //

  virtual base::string16 GetLocalizedRoleDescriptionForUnlabeledImage()
      const = 0;
  virtual base::string16 GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const = 0;
  virtual base::string16 GetLocalizedStringForLandmarkType() const = 0;
  virtual base::string16 GetStyleNameAttributeAsLocalizedString() const = 0;

  //
  // Testing.
  //

  // Accessibility objects can have the "hot tracked" state set when
  // the mouse is hovering over them, but this makes tests flaky because
  // the test behaves differently when the mouse happens to be over an
  // element. The default value should be false if not in testing mode.
  virtual bool ShouldIgnoreHoveredStateForTesting() = 0;

 protected:
  AXPlatformNodeDelegate() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeDelegate);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_H_
