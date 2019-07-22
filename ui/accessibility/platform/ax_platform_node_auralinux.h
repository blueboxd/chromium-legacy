// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_AURALINUX_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_AURALINUX_H_

#include <atk/atk.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/protected_memory.h"
#include "base/optional.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"

// This deleter is used in order to ensure that we properly always free memory
// used by AtkAttributeSet.
struct AtkAttributeSetDeleter {
  void operator()(AtkAttributeSet* attributes) {
    atk_attribute_set_free(attributes);
  }
};

using AtkAttributes = std::unique_ptr<AtkAttributeSet, AtkAttributeSetDeleter>;

// Some ATK interfaces require returning a (const gchar*), use
// this macro to make it safe to return a pointer to a temporary
// string.
#define ATK_AURALINUX_RETURN_STRING(str_expr) \
  {                                           \
    static std::string result;                \
    result = (str_expr);                      \
    return result.c_str();                    \
  }

namespace ui {

// AtkTableCell was introduced in ATK 2.12. Ubuntu Trusty has ATK 2.10.
// Compile-time checks are in place for ATK versions that are older than 2.12.
// However, we also need runtime checks in case the version we are building
// against is newer than the runtime version. To prevent a runtime error, we
// check that we have a version of ATK that supports AtkTableCell. If we do,
// we dynamically load the symbol; if we don't, the interface is absent from
// the accessible object and its methods will not be exposed or callable.
// The definitions below ensure we have no missing symbols. Note that in
// environments where we have ATK > 2.12, the definitions of AtkTableCell and
// AtkTableCellIface below are overridden by the runtime version.
// TODO(accessibility) Remove AtkTableCellInterface when 2.12 is the minimum
// supported version.
struct AX_EXPORT AtkTableCellInterface {
  typedef struct _AtkTableCell AtkTableCell;
  typedef struct _AtkTableCellIface AtkTableCellIface;
  typedef GType (*GetTypeFunc)();
  typedef GPtrArray* (*GetColumnHeaderCellsFunc)(AtkTableCell* cell);
  typedef GPtrArray* (*GetRowHeaderCellsFunc)(AtkTableCell* cell);
  typedef bool (*GetRowColumnSpanFunc)(AtkTableCell* cell,
                                       gint* row,
                                       gint* column,
                                       gint* row_span,
                                       gint* col_span);

  base::ProtectedMemory<GetTypeFunc>* GetType = nullptr;
  base::ProtectedMemory<GetColumnHeaderCellsFunc>* GetColumnHeaderCells =
      nullptr;
  base::ProtectedMemory<GetRowHeaderCellsFunc>* GetRowHeaderCells = nullptr;
  base::ProtectedMemory<GetRowColumnSpanFunc>* GetRowColumnSpan = nullptr;
  bool initialized = false;

  static base::Optional<AtkTableCellInterface> Get();
};

// Implements accessibility on Aura Linux using ATK.
class AX_EXPORT AXPlatformNodeAuraLinux : public AXPlatformNodeBase {
 public:
  AXPlatformNodeAuraLinux();
  ~AXPlatformNodeAuraLinux() override;

  // Set or get the root-level Application object that's the parent of all
  // top-level windows.
  static void SetApplication(AXPlatformNode* application);
  static AXPlatformNode* application();

  static void EnsureGTypeInit();

  // Do asynchronous static initialization.
  static void StaticInitialize();

  // EnsureAtkObjectIsValid will destroy and recreate |atk_object_| if the
  // interface mask is different. This partially relies on looking at the tree's
  // structure. This must not be called when the tree is unstable e.g. in the
  // middle of being unserialized.
  void EnsureAtkObjectIsValid();
  void Destroy() override;

  AtkRole GetAtkRole();
  void GetAtkState(AtkStateSet* state_set);
  AtkRelationSet* GetAtkRelations();
  void GetExtents(gint* x, gint* y, gint* width, gint* height,
                  AtkCoordType coord_type);
  void GetPosition(gint* x, gint* y, AtkCoordType coord_type);
  void GetSize(gint* width, gint* height);
  gfx::NativeViewAccessible HitTestSync(gint x,
                                        gint y,
                                        AtkCoordType coord_type);
  bool GrabFocus();
  bool GrabFocusOrSetSequentialFocusNavigationStartingPointAtOffset(int offset);
  bool GrabFocusOrSetSequentialFocusNavigationStartingPoint();
  bool DoDefaultAction();
  const gchar* GetDefaultActionName();
  AtkAttributeSet* GetAtkAttributes();

  gfx::Vector2d GetParentOriginInScreenCoordinates() const;
  gfx::Vector2d GetParentFrameOriginInScreenCoordinates() const;
  gfx::Rect GetExtentsRelativeToAtkCoordinateType(
      AtkCoordType coord_type) const;

  // AtkDocument helpers
  const gchar* GetDocumentAttributeValue(const gchar* attribute) const;
  AtkAttributeSet* GetDocumentAttributes() const;

  // AtkHyperlink helpers
  AtkHyperlink* GetAtkHyperlink();

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 30, 0)
  void ScrollToPoint(AtkCoordType atk_coord_type, int x, int y);
  void ScrollNodeIntoView(AtkScrollType atk_scroll_type);
#endif  // defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 30, 0)

  // Misc helpers
  void GetFloatAttributeInGValue(ax::mojom::FloatAttribute attr, GValue* value);

  // Event helpers
  void OnActiveDescendantChanged();
  void OnCheckedStateChanged();
  void OnExpandedStateChanged(bool is_expanded);
  void OnFocused();
  void OnWindowActivated();
  void OnWindowDeactivated();
  void OnMenuPopupStart();
  void OnMenuPopupHide();
  void OnMenuPopupEnd();
  void OnSelected();
  void OnSelectedChildrenChanged();
  void OnTextSelectionChanged();
  void OnValueChanged();
  void OnNameChanged();
  void OnDescriptionChanged();
  void OnInvalidStatusChanged();
  void OnDocumentTitleChanged();
  void OnSubtreeCreated();
  void OnSubtreeWillBeDeleted();
  void OnWindowVisibilityChanged();

  bool SupportsSelectionWithAtkSelection();
  bool SelectionAndFocusAreTheSame();

  // AXPlatformNode overrides.
  // This has a side effect of creating the AtkObject if one does not already
  // exist.
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void NotifyAccessibilityEvent(ax::mojom::Event event_type) override;

  // AXPlatformNodeBase overrides.
  void Init(AXPlatformNodeDelegate* delegate) override;
  int GetIndexInParent() override;
  base::string16 GetHypertext() const override;

  void UpdateHypertext();
  const AXHypertext& GetAXHypertext();
  const base::OffsetAdjuster::Adjustments& GetHypertextAdjustments();
  size_t UTF16ToUnicodeOffsetInText(size_t utf16_offset);
  size_t UnicodeToUTF16OffsetInText(int unicode_offset);

  void SetEmbeddedDocument(AtkObject* new_document);
  void SetEmbeddingWindow(AtkObject* new_embedding_window);

  int GetCaretOffset();
  bool SetCaretOffset(int offset);
  bool SetTextSelectionForAtkText(int start_offset, int end_offset);
  bool HasSelection();
  void GetSelectionExtents(int* start_offset, int* end_offset);
  gchar* GetSelectionWithText(int* start_offset, int* end_offset);

  // Return the text attributes for this node given an offset. The start
  // and end attributes will be assigned to the start_offset and end_offset
  // pointers if they are non-null. The return value AtkAttributeSet should
  // be freed with atk_attribute_set_free.
  AtkAttributeSet* GetTextAttributes(int offset,
                                     int* start_offset,
                                     int* end_offset);

  // Return the default text attributes for this node. The default text
  // attributes are the ones that apply to the entire node. Attributes found at
  // a given offset can be thought of as overriding the default attribute.
  // The return value AtkAttributeSet should be freed with
  // atk_attribute_set_free.
  AtkAttributeSet* GetDefaultTextAttributes();

  std::string accessible_name_;

 protected:
  // Offsets for the AtkText API are calculated in UTF-16 code point offsets,
  // but the ATK APIs want all offsets to be in "characters," which we
  // understand to be Unicode character offsets. We keep a lazily generated set
  // of Adjustments to convert between UTF-16 and Unicode character offsets.
  base::Optional<base::OffsetAdjuster::Adjustments> text_unicode_adjustments_ =
      base::nullopt;

  void AddAttributeToList(const char* name,
                          const char* value,
                          PlatformAttributeList* attributes) override;

 private:
  using AXPositionInstance = AXNodePosition::AXPositionInstance;
  using AXPositionInstanceType = typename AXPositionInstance::element_type;
  using AXNodeRange = AXRange<AXPositionInstanceType>;

  enum AtkInterfaces {
    ATK_ACTION_INTERFACE,
    ATK_COMPONENT_INTERFACE,
    ATK_DOCUMENT_INTERFACE,
    ATK_EDITABLE_TEXT_INTERFACE,
    ATK_HYPERLINK_INTERFACE,
    ATK_HYPERTEXT_INTERFACE,
    ATK_IMAGE_INTERFACE,
    ATK_SELECTION_INTERFACE,
    ATK_TABLE_INTERFACE,
    ATK_TABLE_CELL_INTERFACE,
    ATK_TEXT_INTERFACE,
    ATK_VALUE_INTERFACE,
    ATK_WINDOW_INTERFACE,
  };

  int GetGTypeInterfaceMask();
  GType GetAccessibilityGType();
  AtkObject* CreateAtkObject();
  gfx::NativeViewAccessible GetOrCreateAtkObject();
  void DestroyAtkObjects();
  void AddRelationToSet(AtkRelationSet*,
                        AtkRelationType,
                        AXPlatformNode* target);
  bool IsInLiveRegion();
  base::Optional<std::pair<int, int>> GetEmbeddedObjectIndicesForId(int id);

  void ComputeStylesIfNeeded();
  void MergeSpellingIntoAtkTextAttributesAtOffset(
      int offset,
      std::map<int, AtkAttributes>* text_attributes);
  AtkAttributes ComputeTextAttributes() const;
  int FindStartOfStyle(int start_offset, ui::TextBoundaryDirection direction);

  // The AtkStateType for a checkable node can vary depending on the role.
  AtkStateType GetAtkStateTypeForCheckableNode();

  // Keep information of latest AtkInterfaces mask to refresh atk object
  // interfaces accordingly if needed.
  int interface_mask_ = 0;

  // We own a reference to these ref-counted objects.
  AtkObject* atk_object_ = nullptr;
  AtkHyperlink* atk_hyperlink_ = nullptr;

  // Some weak pointers which help us track ATK embeds / embedded by relations.
  AtkObject* embedded_document_ = nullptr;
  AtkObject* embedding_window_ = nullptr;

  // Whether or not this node (if it is a frame or a window) was
  // minimized the last time it's visibility changed.
  bool was_minimized_ = false;

  // The previously observed text selection for this node. We store
  // this in order to avoid sending duplicate text-selection-changed
  // and text-caret-moved events.
  std::pair<int, int> text_selection_ = std::make_pair(-1, -1);

  // A map which converts between an offset in the node's hypertext and the
  // ATK text attributes at that offset.
  std::map<int, AtkAttributes> offset_to_text_attributes_;

  // The default ATK text attributes for this node.
  AtkAttributes default_text_attributes_;

  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeAuraLinux);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_AURALINUX_H_
