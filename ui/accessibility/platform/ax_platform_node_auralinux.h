// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_AURALINUX_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_AURALINUX_H_

#include <atk/atk.h>

#include <string>
#include <utility>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"

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

  GetTypeFunc GetType = nullptr;
  GetColumnHeaderCellsFunc GetColumnHeaderCells = nullptr;
  GetRowHeaderCellsFunc GetRowHeaderCells = nullptr;
  GetRowColumnSpanFunc GetRowColumnSpan = nullptr;
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

  void DataChanged();
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

  void ScrollToPoint(AtkCoordType atk_coord_type, int x, int y);
#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 30, 0)
  base::Optional<gfx::Point> CalculateScrollToPoint(AtkScrollType scroll_type);
#endif  // ATK_230

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
  void DestroyAtkObjects();
  void AddRelationToSet(AtkRelationSet*,
                        AtkRelationType,
                        AXPlatformNode* target);
  bool IsInLiveRegion();

  // The AtkStateType for a checkable node can vary depending on the role.
  AtkStateType GetAtkStateTypeForCheckableNode();

  // Find the topmost document that is an ancestor of this node. Returns
  // null if there is no ancestor which is a document.`
  AXPlatformNodeAuraLinux* FindTopmostDocumentAncestor();

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

  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeAuraLinux);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_AURALINUX_H_
