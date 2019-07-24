// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

#include <dlfcn.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/memory/protected_memory_cfi.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_mode_observer.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/atk_util_auralinux.h"
#include "ui/accessibility/platform/ax_platform_atk_hyperlink.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_base.h"
#include "ui/accessibility/platform/ax_platform_text_boundary.h"
#include "ui/gfx/geometry/rect_conversions.h"

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 10, 0)
#define ATK_210
#endif

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 12, 0)
#define ATK_212
#endif

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 16, 0)
#define ATK_216
#endif

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 26, 0)
#define ATK_226
#endif

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 30, 0)
#define ATK_230
#endif

namespace ui {

namespace {

// When accepting input from clients calling the API, an ATK character offset
// of -1 can often represent the length of the string.
static const int kStringLengthOffset = -1;

// We must forward declare this because it is used by the traditional GObject
// type manipulation macros.
namespace atk_object {
GType GetType();
}  // namespace atk_object

//
// ax_platform_node_auralinux AtkObject definition and implementation.
//
#define AX_PLATFORM_NODE_AURALINUX_TYPE (atk_object::GetType())
#define AX_PLATFORM_NODE_AURALINUX(obj)                               \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), AX_PLATFORM_NODE_AURALINUX_TYPE, \
                              AXPlatformNodeAuraLinuxObject))
#define AX_PLATFORM_NODE_AURALINUX_CLASS(klass)                      \
  (G_TYPE_CHECK_CLASS_CAST((klass), AX_PLATFORM_NODE_AURALINUX_TYPE, \
                           AXPlatformNodeAuraLinuxClass))
#define IS_AX_PLATFORM_NODE_AURALINUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), AX_PLATFORM_NODE_AURALINUX_TYPE))
#define IS_AX_PLATFORM_NODE_AURALINUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), AX_PLATFORM_NODE_AURALINUX_TYPE))
#define AX_PLATFORM_NODE_AURALINUX_GET_CLASS(obj)                    \
  (G_TYPE_INSTANCE_GET_CLASS((obj), AX_PLATFORM_NODE_AURALINUX_TYPE, \
                             AXPlatformNodeAuraLinuxClass))

typedef struct _AXPlatformNodeAuraLinuxObject AXPlatformNodeAuraLinuxObject;
typedef struct _AXPlatformNodeAuraLinuxClass AXPlatformNodeAuraLinuxClass;

struct _AXPlatformNodeAuraLinuxObject {
  AtkObject parent;
  AXPlatformNodeAuraLinux* m_object;
};

struct _AXPlatformNodeAuraLinuxClass {
  AtkObjectClass parent_class;
};

// The root-level Application object that's the parent of all top-level windows.
AXPlatformNode* g_root_application = nullptr;

// The last AtkObject with keyboard focus. Tracking this is required to emit the
// ATK_STATE_FOCUSED change to false.
AtkObject* g_current_focused = nullptr;

// The last AtkObject which was the active descendant in the currently-focused
// object (example: The highlighted option within a focused select element).
// As with g_current_focused, we track this to emit events when this object is
// no longer the active descendant.
AtkObject* g_current_active_descendant = nullptr;

// The last object which was selected. Tracking this is required because
// widgets in the browser UI only emit notifications upon becoming selected,
// but clients also expect notifications when items become unselected.
AXPlatformNodeAuraLinux* g_current_selected = nullptr;

// The AtkObject with role=ATK_ROLE_FRAME that represents the toplevel desktop
// window with focus. If this window is not one of our windows, this value
// should be null. This is a weak pointer as well, so its value will also be
// null if if the AtkObject is destroyed.
AtkObject* g_active_top_level_frame = nullptr;

#if defined(ATK_216)
constexpr AtkRole kStaticRole = ATK_ROLE_STATIC;
constexpr AtkRole kSubscriptRole = ATK_ROLE_SUBSCRIPT;
constexpr AtkRole kSuperscriptRole = ATK_ROLE_SUPERSCRIPT;
#else
constexpr AtkRole kStaticRole = ATK_ROLE_TEXT;
constexpr AtkRole kSubscriptRole = ATK_ROLE_TEXT;
constexpr AtkRole kSuperscriptRole = ATK_ROLE_TEXT;
#endif

#if defined(ATK_226)
constexpr AtkRole kAtkFootnoteRole = ATK_ROLE_FOOTNOTE;
#else
constexpr AtkRole kAtkFootnoteRole = ATK_ROLE_LIST_ITEM;
#endif

AXPlatformNodeAuraLinux* AtkObjectToAXPlatformNodeAuraLinux(
    AtkObject* atk_object) {
  if (!atk_object)
    return nullptr;

  if (IS_AX_PLATFORM_NODE_AURALINUX(atk_object)) {
    AXPlatformNodeAuraLinuxObject* platform_object =
        AX_PLATFORM_NODE_AURALINUX(atk_object);
    return platform_object->m_object;
  }

  return nullptr;
}

// The ATK API often requires pointers to be used as out arguments, while
// allowing for those pointers to be null if the caller is not interested in
// the value. This function is a simpler helper to avoid continually checking
// for null and to help prevent forgetting to check for null.
void SetIntPointerValueIfNotNull(int* pointer, int value) {
  if (pointer)
    *pointer = value;
}

bool SupportsAtkComponentScrollingInterface() {
  return dlsym(RTLD_DEFAULT, "atk_component_scroll_to_point");
}

AtkObject* FindAtkObjectParentFrame(AtkObject* atk_object) {
  while (atk_object) {
    if (atk_object_get_role(atk_object) == ATK_ROLE_FRAME)
      return atk_object;
    atk_object = atk_object_get_parent(atk_object);
  }
  return nullptr;
}

bool EmitsAtkTextEvents(AtkObject* atk_object) {
  // If this node is not a static text node, it supports the full AtkText
  // interface.
  AtkRole role = atk_object_get_role(atk_object);
  if (role != ATK_ROLE_TEXT)
    return true;

  // If this node is not a static text leaf node, it supports the full AtkText
  // interface.
  if (atk_object_get_n_accessible_children(atk_object))
    return true;

  // If this node is an anonymous block that is a static text leaf node, it
  // should also emit events. The heuristic that Orca uses for this is to check
  // whether or not it has any non-static-text siblings. We duplicate that here
  // to maintain compatibility.
  AtkObject* parent = atk_object_get_parent(atk_object);
  if (!parent)
    return false;

  int num_siblings = atk_object_get_n_accessible_children(parent);
  for (int i = 0; i < num_siblings; i++) {
    AtkObject* sibling = atk_object_ref_accessible_child(parent, i);
    AtkRole role = atk_object_get_role(sibling);
    g_object_unref(sibling);
    if (role != ATK_ROLE_TEXT)
      return true;
  }

  return false;
}

bool IsFrameAncestorOfAtkObject(AtkObject* frame, AtkObject* atk_object) {
  AtkObject* current_frame = FindAtkObjectParentFrame(atk_object);
  while (current_frame) {
    if (current_frame == frame)
      return true;
    current_frame =
        FindAtkObjectParentFrame(atk_object_get_parent(current_frame));
  }
  return false;
}

// Returns a stack of AtkObjects of activated popup menus. Since each popup
// menu and submenu has its own native window, we want to properly manage the
// activated state for their containing frames.
std::vector<AtkObject*>& GetActiveMenus() {
  static base::NoDestructor<std::vector<AtkObject*>> active_menus;
  return *active_menus;
}

// The currently active frame is g_active_top_level_frame, unless there is an
// active menu. If there is an active menu the parent frame of the
// most-recently opened active menu should be the currently active frame.
AtkObject* ComputeActiveTopLevelFrame() {
  if (!GetActiveMenus().empty())
    return FindAtkObjectParentFrame(GetActiveMenus().back());
  return g_active_top_level_frame;
}

const char* GetUniqueAccessibilityGTypeName(int interface_mask) {
  // 37 characters is enough for "AXPlatformNodeAuraLinux%x" with any integer
  // value.
  static char name[37];
  snprintf(name, sizeof(name), "AXPlatformNodeAuraLinux%x", interface_mask);
  return name;
}

void SetWeakGPtrToAtkObject(AtkObject** weak_pointer, AtkObject* new_value) {
  if (*weak_pointer == new_value)
    return;

  if (*weak_pointer) {
    g_object_remove_weak_pointer(G_OBJECT(*weak_pointer),
                                 reinterpret_cast<void**>(weak_pointer));
  }

  *weak_pointer = new_value;

  if (new_value) {
    g_object_add_weak_pointer(G_OBJECT(new_value),
                              reinterpret_cast<void**>(weak_pointer));
  }
}

void SetActiveTopLevelFrame(AtkObject* new_top_level_frame) {
  SetWeakGPtrToAtkObject(&g_active_top_level_frame, new_top_level_frame);
}

AXCoordinateSystem AtkCoordTypeToAXCoordinateSystem(
    AtkCoordType coordinate_type) {
  switch (coordinate_type) {
    case ATK_XY_SCREEN:
      return AXCoordinateSystem::kScreen;
    case ATK_XY_WINDOW:
      return AXCoordinateSystem::kRootFrame;
#if defined(ATK_230)
    case ATK_XY_PARENT:
      // AXCoordinateSystem does not support parent coordinates.
      NOTIMPLEMENTED();
      return AXCoordinateSystem::kFrame;
#endif
    default:
      return AXCoordinateSystem::kScreen;
  }
}

const char* BuildDescriptionFromHeaders(AXPlatformNodeDelegate* delegate,
                                        const std::vector<int32_t>& ids) {
  std::vector<std::string> names;
  for (const auto& node_id : ids) {
    if (AXPlatformNode* header = delegate->GetFromNodeID(node_id)) {
      if (AtkObject* atk_header = header->GetNativeViewAccessible())
        names.push_back(atk_object_get_name(atk_header));
    }
  }

  std::string result = base::JoinString(names, " ");

#if defined(LEAK_SANITIZER) && !defined(OS_NACL)
  // http://crbug.com/982839
  // atk_table_get_column_description and atk_table_get_row_description return
  // const gchar*, which suggests the caller does not gain ownership of the
  // returned string. The g_strdup below causes a new allocation, which does not
  // fit that pattern and causes a leak in tests.
  ScopedLeakSanitizerDisabler lsan_disabler;
#endif

  return g_strdup(result.c_str());
}

gfx::Point FindAtkObjectParentCoords(AtkObject* atk_object) {
  if (!atk_object)
    return gfx::Point(0, 0);

  if (atk_object_get_role(atk_object) == ATK_ROLE_FRAME) {
    int x, y;
    atk_component_get_extents(ATK_COMPONENT(atk_object), &x, &y, nullptr,
                              nullptr, ATK_XY_WINDOW);
    gfx::Point window_coords(x, y);
    return window_coords;
  }
  atk_object = atk_object_get_parent(atk_object);

  return FindAtkObjectParentCoords(atk_object);
}

AtkAttributeSet* PrependAtkAttributeToAtkAttributeSet(
    const char* name,
    const char* value,
    AtkAttributeSet* attribute_set) {
  AtkAttribute* attribute =
      static_cast<AtkAttribute*>(g_malloc(sizeof(AtkAttribute)));
  attribute->name = g_strdup(name);
  attribute->value = g_strdup(value);
  return g_slist_prepend(attribute_set, attribute);
}

AtkObject* GetActiveDescendantOfCurrentFocused() {
  if (!g_current_focused)
    return nullptr;

  auto* node = AtkObjectToAXPlatformNodeAuraLinux(g_current_focused);
  if (!node)
    return nullptr;

  int32_t id =
      node->GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId);
  if (auto* descendant = node->GetDelegate()->GetFromNodeID(id))
    return descendant->GetNativeViewAccessible();

  return nullptr;
}
bool SelectionOffsetsIndicateSelection(const std::pair<int, int>& offsets) {
  return offsets.first >= 0 && offsets.second >= 0 &&
         offsets.first != offsets.second;
}

void AddTextAttributeToSet(const AtkTextAttribute attribute,
                           const std::string& value,
                           AtkAttributeSet** attributes) {
  AtkAttribute* new_attribute =
      static_cast<AtkAttribute*>(g_malloc(sizeof(AtkAttribute)));
  new_attribute->name = g_strdup(atk_text_attribute_get_name(attribute));
  new_attribute->value = g_strdup(value.c_str());
  *attributes = g_slist_prepend(*attributes, new_attribute);
}

bool HasInvalidAttributeInSet(AtkAttributeSet* attributes) {
  const char* underline_attribute =
      atk_text_attribute_get_name(ATK_TEXT_ATTR_UNDERLINE);
  DCHECK(underline_attribute);

  AtkAttributeSet* current = attributes;
  while (current) {
    const AtkAttribute* attribute = static_cast<AtkAttribute*>(current->data);
    if (!g_strcmp0(attribute->name, underline_attribute) &&
        !g_strcmp0(attribute->value, "error")) {
      return true;
    }
    current = g_slist_next(current);
  }
  return false;
}

bool AttributeSetsEqual(AtkAttributeSet* one, AtkAttributeSet* two) {
  AtkAttributeSet* current_one = one;
  AtkAttributeSet* current_two = two;

  while (current_one && current_two) {
    const AtkAttribute* attribute_one =
        static_cast<AtkAttribute*>(current_one->data);
    const AtkAttribute* attribute_two =
        static_cast<AtkAttribute*>(current_two->data);

    if (g_strcmp0(attribute_one->name, attribute_two->name) ||
        g_strcmp0(attribute_one->value, attribute_two->value)) {
      return false;
    }

    current_one = g_slist_next(current_one);
    current_two = g_slist_next(current_two);
  }

  // The sets are only equal if they have the same length, which means that we
  // need to have iterated to the end of both sets.
  return !current_one && !current_two;
}

AtkAttributeSet* CopyAttributeSet(AtkAttributeSet* attributes) {
  AtkAttributeSet* copied_attributes = nullptr;
  while (attributes) {
    const AtkAttribute* attribute =
        static_cast<const AtkAttribute*>(attributes->data);
    AtkAttribute* new_attribute =
        static_cast<AtkAttribute*>(g_malloc(sizeof(AtkAttribute)));
    new_attribute->name = g_strdup(attribute->name);
    new_attribute->value = g_strdup(attribute->value);
    copied_attributes = g_slist_prepend(copied_attributes, new_attribute);

    attributes = g_slist_next(attributes);
  }

  return g_slist_reverse(copied_attributes);
}

namespace atk_component {

void GetExtents(AtkComponent* atk_component,
                gint* x,
                gint* y,
                gint* width,
                gint* height,
                AtkCoordType coord_type) {
  g_return_if_fail(ATK_IS_COMPONENT(atk_component));

  if (x)
    *x = 0;
  if (y)
    *y = 0;
  if (width)
    *width = 0;
  if (height)
    *height = 0;

  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetExtents(x, y, width, height, coord_type);
}

void GetPosition(AtkComponent* atk_component,
                 gint* x,
                 gint* y,
                 AtkCoordType coord_type) {
  g_return_if_fail(ATK_IS_COMPONENT(atk_component));

  if (x)
    *x = 0;
  if (y)
    *y = 0;

  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetPosition(x, y, coord_type);
}

void GetSize(AtkComponent* atk_component, gint* width, gint* height) {
  g_return_if_fail(ATK_IS_COMPONENT(atk_component));

  if (width)
    *width = 0;
  if (height)
    *height = 0;

  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetSize(width, height);
}

AtkObject* RefAccesibleAtPoint(AtkComponent* atk_component,
                               gint x,
                               gint y,
                               AtkCoordType coord_type) {
  g_return_val_if_fail(ATK_IS_COMPONENT(atk_component), nullptr);
  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  AtkObject* result = obj->HitTestSync(x, y, coord_type);
  if (result)
    g_object_ref(result);
  return result;
}

gboolean GrabFocus(AtkComponent* atk_component) {
  g_return_val_if_fail(ATK_IS_COMPONENT(atk_component), FALSE);
  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return FALSE;

  return obj->GrabFocus();
}

#if defined(ATK_230)
gboolean ScrollTo(AtkComponent* component, AtkScrollType scroll_type) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(component));
  if (!obj)
    return FALSE;

  obj->ScrollNodeIntoView(scroll_type);
  return TRUE;
}

gboolean ScrollToPoint(AtkComponent* component,
                       AtkCoordType atk_coord_type,
                       gint x,
                       gint y) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(component));
  if (!obj)
    return FALSE;

  obj->ScrollToPoint(atk_coord_type, x, y);
  return TRUE;
}
#endif

void Init(AtkComponentIface* iface) {
  iface->get_extents = GetExtents;
  iface->get_position = GetPosition;
  iface->get_size = GetSize;
  iface->ref_accessible_at_point = RefAccesibleAtPoint;
  iface->grab_focus = GrabFocus;
#if defined(ATK_230)
  if (SupportsAtkComponentScrollingInterface()) {
    iface->scroll_to = ScrollTo;
    iface->scroll_to_point = ScrollToPoint;
  }
#endif
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_component

namespace atk_action {

gboolean DoAction(AtkAction* atk_action, gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), FALSE);
  g_return_val_if_fail(!index, FALSE);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return FALSE;

  return obj->DoDefaultAction();
}

gint GetNActions(AtkAction* atk_action) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), 0);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  return 1;
}

const gchar* GetDescription(AtkAction*, gint) {
  // Not implemented. Right now Orca does not provide this and
  // Chromium is not providing a string for the action description.
  return nullptr;
}

const gchar* GetName(AtkAction* atk_action, gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), nullptr);
  g_return_val_if_fail(!index, nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetDefaultActionName();
}

const gchar* GetKeybinding(AtkAction* atk_action, gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), nullptr);
  g_return_val_if_fail(!index, nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetStringAttribute(ax::mojom::StringAttribute::kAccessKey)
      .c_str();
}

void Init(AtkActionIface* iface) {
  iface->do_action = DoAction;
  iface->get_n_actions = GetNActions;
  iface->get_description = GetDescription;
  iface->get_name = GetName;
  iface->get_keybinding = GetKeybinding;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_action

namespace atk_document {

const gchar* GetDocumentAttributeValue(AtkDocument* atk_doc,
                                       const gchar* attribute) {
  g_return_val_if_fail(ATK_IS_DOCUMENT(atk_doc), nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_doc);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetDocumentAttributeValue(attribute);
}

AtkAttributeSet* GetDocumentAttributes(AtkDocument* atk_doc) {
  g_return_val_if_fail(ATK_IS_DOCUMENT(atk_doc), 0);

  AtkObject* atk_object = ATK_OBJECT(atk_doc);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetDocumentAttributes();
}

void Init(AtkDocumentIface* iface) {
  iface->get_document_attribute_value = GetDocumentAttributeValue;
  iface->get_document_attributes = GetDocumentAttributes;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_document

namespace atk_image {

void GetImagePosition(AtkImage* atk_img,
                      gint* x,
                      gint* y,
                      AtkCoordType coord_type) {
  g_return_if_fail(ATK_IMAGE(atk_img));

  AtkObject* atk_object = ATK_OBJECT(atk_img);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetPosition(x, y, coord_type);
}

const gchar* GetImageDescription(AtkImage* atk_img) {
  g_return_val_if_fail(ATK_IMAGE(atk_img), nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_img);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetStringAttribute(ax::mojom::StringAttribute::kDescription)
      .c_str();
}

void GetImageSize(AtkImage* atk_img, gint* width, gint* height) {
  g_return_if_fail(ATK_IMAGE(atk_img));

  AtkObject* atk_object = ATK_OBJECT(atk_img);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetSize(width, height);
}

void Init(AtkImageIface* iface) {
  iface->get_image_position = GetImagePosition;
  iface->get_image_description = GetImageDescription;
  iface->get_image_size = GetImageSize;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_image

namespace atk_value {

void GetCurrentValue(AtkValue* atk_value, GValue* value) {
  g_return_if_fail(ATK_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kValueForRange,
                                 value);
}

void GetMinimumValue(AtkValue* atk_value, GValue* value) {
  g_return_if_fail(ATK_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kMinValueForRange,
                                 value);
}

void GetMaximumValue(AtkValue* atk_value, GValue* value) {
  g_return_if_fail(ATK_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kMaxValueForRange,
                                 value);
}

void GetMinimumIncrement(AtkValue* atk_value, GValue* value) {
  g_return_if_fail(ATK_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kStepValueForRange,
                                 value);
}

void Init(AtkValueIface* iface) {
  iface->get_current_value = GetCurrentValue;
  iface->get_maximum_value = GetMaximumValue;
  iface->get_minimum_value = GetMinimumValue;
  iface->get_minimum_increment = GetMinimumIncrement;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_value

namespace atk_hyperlink {

AtkHyperlink* GetHyperlink(AtkHyperlinkImpl* atk_hyperlink_impl) {
  g_return_val_if_fail(ATK_HYPERLINK_IMPL(atk_hyperlink_impl), 0);

  AtkObject* atk_object = ATK_OBJECT(atk_hyperlink_impl);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  AtkHyperlink* atk_hyperlink = obj->GetAtkHyperlink();
  g_object_ref(atk_hyperlink);

  return atk_hyperlink;
}

void Init(AtkHyperlinkImplIface* iface) {
  iface->get_hyperlink = GetHyperlink;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_hyperlink

namespace atk_hypertext {

AtkHyperlink* GetLink(AtkHypertext* hypertext, int index) {
  g_return_val_if_fail(ATK_HYPERTEXT(hypertext), 0);
  auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(hypertext));
  if (!obj)
    return nullptr;

  const AXHypertext& ax_hypertext = obj->GetAXHypertext();
  if (index > static_cast<int>(ax_hypertext.hyperlinks.size()) || index < 0)
    return nullptr;

  int32_t id = ax_hypertext.hyperlinks[index];
  auto* link = static_cast<AXPlatformNodeAuraLinux*>(
      AXPlatformNodeBase::GetFromUniqueId(id));
  if (!link)
    return nullptr;

  return link->GetAtkHyperlink();
}

int GetNLinks(AtkHypertext* hypertext) {
  g_return_val_if_fail(ATK_HYPERTEXT(hypertext), 0);
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(hypertext));
  return obj ? obj->GetAXHypertext().hyperlinks.size() : 0;
}

int GetLinkIndex(AtkHypertext* hypertext, int char_index) {
  g_return_val_if_fail(ATK_HYPERTEXT(hypertext), 0);
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(hypertext));
  if (!obj)
    return -1;

  auto it = obj->GetAXHypertext().hyperlink_offset_to_index.find(char_index);
  if (it == obj->GetAXHypertext().hyperlink_offset_to_index.end())
    return -1;
  return it->second;
}

void Init(AtkHypertextIface* iface) {
  iface->get_link = GetLink;
  iface->get_n_links = GetNLinks;
  iface->get_link_index = GetLinkIndex;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_hypertext

namespace atk_text {

gchar* GetText(AtkText* atk_text, gint start_offset, gint end_offset) {
  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  base::string16 text = obj->GetHypertext();

  start_offset = obj->UnicodeToUTF16OffsetInText(start_offset);
  if (start_offset < 0 || start_offset >= static_cast<int>(text.size()))
    return nullptr;

  if (end_offset < 0) {
    end_offset = text.size();
  } else {
    end_offset = obj->UnicodeToUTF16OffsetInText(end_offset);
    end_offset = std::max(start_offset,
                          std::min(end_offset, static_cast<int>(text.size())));
  }

  DCHECK_GE(start_offset, 0);
  DCHECK_GE(end_offset, start_offset);

  return g_strdup(
      base::UTF16ToUTF8(text.substr(start_offset, end_offset - start_offset))
          .c_str());
}

gint GetCharacterCount(AtkText* atk_text) {
  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  return obj->UTF16ToUnicodeOffsetInText(obj->GetHypertext().length());
}

gunichar GetCharacterAtOffset(AtkText* atk_text, int offset) {
  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  base::string16 text = obj->GetHypertext();
  int32_t text_length = text.length();

  offset = obj->UnicodeToUTF16OffsetInText(offset);
  int32_t limited_offset = std::max(0, std::min(text_length, offset));

  uint32_t code_point;
  base::ReadUnicodeCharacter(text.c_str(), text_length + 1, &limited_offset,
                             &code_point);
  return code_point;
}

// This function returns a single character as a UTF-8 encoded C string because
// the character may be encoded into more than one byte.
char* GetCharacter(AtkText* atk_text,
                   int offset,
                   int* start_offset,
                   int* end_offset) {
  *start_offset = -1;
  *end_offset = -1;

  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  if (offset < 0 || offset >= GetCharacterCount(atk_text))
    return nullptr;

  char* text = GetText(atk_text, offset, offset + 1);
  if (!text)
    return nullptr;

  *start_offset = offset;
  *end_offset = offset + 1;
  return text;
}

char* GetTextWithBoundaryType(AtkText* atk_text,
                              int offset,
                              AXTextBoundary boundary,
                              int* start_offset_ptr,
                              int* end_offset_ptr) {
  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  if (offset < 0 || offset >= atk_text_get_character_count(atk_text))
    return nullptr;

  // The offset that we receive from the API is a Unicode character offset.
  // Since we calculate boundaries in terms of UTF-16 code point offsets, we
  // need to convert this input value.
  offset = obj->UnicodeToUTF16OffsetInText(offset);

  int start_offset =
      obj->FindTextBoundary(boundary, offset, BACKWARDS_DIRECTION);
  int end_offset = obj->FindTextBoundary(boundary, offset, FORWARDS_DIRECTION);
  if (start_offset < 0 || end_offset < 0)
    return nullptr;

  DCHECK_LE(start_offset, end_offset)
      << "Start offset should be less than or equal the end offset.";

  // The ATK API is also expecting Unicode character offsets as output
  // values.
  *start_offset_ptr = obj->UTF16ToUnicodeOffsetInText(start_offset);
  *end_offset_ptr = obj->UTF16ToUnicodeOffsetInText(end_offset);

  base::string16 text = obj->GetHypertext();
  DCHECK_LE(end_offset, static_cast<int>(text.size()));

  base::string16 substr = text.substr(start_offset, end_offset - start_offset);
  return g_strdup(base::UTF16ToUTF8(substr).c_str());
}

char* GetTextAtOffset(AtkText* atk_text,
                      int offset,
                      AtkTextBoundary atk_boundary,
                      int* start_offset,
                      int* end_offset) {
  if (atk_boundary == ATK_TEXT_BOUNDARY_CHAR) {
    return GetCharacter(atk_text, offset, start_offset, end_offset);
  }

  AXTextBoundary boundary = FromAtkTextBoundary(atk_boundary);
  return GetTextWithBoundaryType(atk_text, offset, boundary, start_offset,
                                 end_offset);
}

char* GetTextAfterOffset(AtkText* atk_text,
                         int offset,
                         AtkTextBoundary boundary,
                         int* start_offset,
                         int* end_offset) {
  if (boundary != ATK_TEXT_BOUNDARY_CHAR) {
    *start_offset = -1;
    *end_offset = -1;
    return nullptr;
  }

  // ATK does not offer support for the special negative index and we don't
  // want to do arithmetic on that value below.
  if (offset == kStringLengthOffset)
    return nullptr;

  return GetCharacter(atk_text, offset + 1, start_offset, end_offset);
}

char* GetTextBeforeOffset(AtkText* atk_text,
                          int offset,
                          AtkTextBoundary boundary,
                          int* start_offset,
                          int* end_offset) {
  if (boundary != ATK_TEXT_BOUNDARY_CHAR) {
    *start_offset = -1;
    *end_offset = -1;
    return nullptr;
  }

  // ATK does not offer support for the special negative index and we don't
  // want to do arithmetic on that value below.
  if (offset == kStringLengthOffset)
    return nullptr;

  return GetCharacter(atk_text, offset - 1, start_offset, end_offset);
}

gint GetCaretOffset(AtkText* atk_text) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (!obj)
    return -1;
  return obj->GetCaretOffset();
}

gboolean SetCaretOffset(AtkText* atk_text, gint offset) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (!obj)
    return FALSE;
  if (!obj->SetCaretOffset(offset))
    return FALSE;

  // Orca expects atk_text_set_caret_offset to either focus the target element
  // or set the sequential focus navigation starting point there.
  int utf16_offset = obj->UnicodeToUTF16OffsetInText(offset);
  obj->GrabFocusOrSetSequentialFocusNavigationStartingPointAtOffset(
      utf16_offset);

  return TRUE;
}

int GetNSelections(AtkText* atk_text) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (!obj)
    return 0;

  // We only support a single selection.
  return obj->HasSelection() ? 1 : 0;
}

gchar* GetSelection(AtkText* atk_text,
                    int selection_num,
                    int* start_offset,
                    int* end_offset) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (!obj)
    return nullptr;
  if (selection_num != 0)
    return nullptr;

  return obj->GetSelectionWithText(start_offset, end_offset);
}

gboolean RemoveSelection(AtkText* atk_text, int selection_num) {
  if (selection_num != 0)
    return FALSE;

  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (!obj)
    return FALSE;

  // Simply collapse the selection to the position of the caret if a caret is
  // visible, otherwise set the selection to 0.
  int selection_end = obj->UTF16ToUnicodeOffsetInText(
      obj->GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd));
  return SetCaretOffset(atk_text, selection_end);
}

gboolean SetSelection(AtkText* atk_text,
                      int selection_num,
                      int start_offset,
                      int end_offset) {
  if (selection_num != 0)
    return FALSE;

  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (!obj)
    return FALSE;

  return obj->SetTextSelectionForAtkText(start_offset, end_offset);
}

gboolean AddSelection(AtkText* atk_text, int start_offset, int end_offset) {
  // We only support one selection.
  return SetSelection(atk_text, 0, start_offset, end_offset);
}

#if defined(ATK_210)
char* GetStringAtOffset(AtkText* atk_text,
                        int offset,
                        AtkTextGranularity atk_granularity,
                        int* start_offset,
                        int* end_offset) {
  *start_offset = -1;
  *end_offset = -1;

  if (atk_granularity == ATK_TEXT_GRANULARITY_CHAR) {
    return GetCharacter(atk_text, offset, start_offset, end_offset);
  }

  AXTextBoundary boundary = FromAtkTextGranularity(atk_granularity);
  return GetTextWithBoundaryType(atk_text, offset, boundary, start_offset,
                                 end_offset);
}
#endif

gfx::Rect GetUnclippedParentHypertextRangeBoundsRect(
    AXPlatformNodeDelegate* ax_platform_node_delegate,
    const int start_offset,
    const int end_offset) {
  const AXPlatformNode* parent_platform_node =
      AXPlatformNode::FromNativeViewAccessible(
          ax_platform_node_delegate->GetParent());
  if (!parent_platform_node)
    return gfx::Rect();

  const AXPlatformNodeDelegate* parent_ax_platform_node_delegate =
      parent_platform_node->GetDelegate();
  if (!parent_ax_platform_node_delegate)
    return gfx::Rect();

  return ax_platform_node_delegate->GetHypertextRangeBoundsRect(
             start_offset, end_offset, AXCoordinateSystem::kRootFrame,
             AXClippingBehavior::kUnclipped) -
         parent_ax_platform_node_delegate
             ->GetBoundsRect(AXCoordinateSystem::kRootFrame,
                             AXClippingBehavior::kClipped)
             .OffsetFromOrigin();
}

void GetCharacterExtents(AtkText* atk_text,
                         int offset,
                         int* x,
                         int* y,
                         int* width,
                         int* height,
                         AtkCoordType coordinate_type) {
  gfx::Rect rect;
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (obj) {
    switch (coordinate_type) {
#if defined(ATK_230)
      case ATK_XY_PARENT:
        rect = GetUnclippedParentHypertextRangeBoundsRect(obj->GetDelegate(),
                                                          offset, offset + 1);
        break;
#endif
      default:
        rect = obj->GetDelegate()->GetHypertextRangeBoundsRect(
            obj->UnicodeToUTF16OffsetInText(offset),
            obj->UnicodeToUTF16OffsetInText(offset + 1),
            AtkCoordTypeToAXCoordinateSystem(coordinate_type),
            AXClippingBehavior::kUnclipped);
        break;
    }
  }

  if (x)
    *x = rect.x();
  if (y)
    *y = rect.y();
  if (width)
    *width = rect.width();
  if (height)
    *height = rect.height();
}

void GetRangeExtents(AtkText* atk_text,
                     int start_offset,
                     int end_offset,
                     AtkCoordType coordinate_type,
                     AtkTextRectangle* out_rectangle) {
  if (!out_rectangle)
    return;

  gfx::Rect rect;
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (obj) {
    switch (coordinate_type) {
#if defined(ATK_230)
      case ATK_XY_PARENT:
        rect = GetUnclippedParentHypertextRangeBoundsRect(
            obj->GetDelegate(), start_offset, end_offset);
        break;
#endif
      default:
        rect = obj->GetDelegate()->GetHypertextRangeBoundsRect(
            obj->UnicodeToUTF16OffsetInText(start_offset),
            obj->UnicodeToUTF16OffsetInText(end_offset),
            AtkCoordTypeToAXCoordinateSystem(coordinate_type),
            AXClippingBehavior::kUnclipped);
        break;
    }
  }

  out_rectangle->x = rect.x();
  out_rectangle->y = rect.y();
  out_rectangle->width = rect.width();
  out_rectangle->height = rect.height();
}

AtkAttributeSet* GetRunAttributes(AtkText* atk_text,
                                  gint offset,
                                  gint* start_offset,
                                  gint* end_offset) {
  SetIntPointerValueIfNotNull(start_offset, -1);
  SetIntPointerValueIfNotNull(end_offset, -1);

  if (offset < 0 || offset > GetCharacterCount(atk_text))
    return nullptr;

  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return CopyAttributeSet(
      obj->GetTextAttributes(offset, start_offset, end_offset));
}

AtkAttributeSet* GetDefaultAttributes(AtkText* atk_text) {
  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;
  return CopyAttributeSet(obj->GetDefaultTextAttributes());
}

void Init(AtkTextIface* iface) {
  iface->get_text = GetText;
  iface->get_character_count = GetCharacterCount;
  iface->get_character_at_offset = GetCharacterAtOffset;
  iface->get_text_after_offset = GetTextAfterOffset;
  iface->get_text_before_offset = GetTextBeforeOffset;
  iface->get_text_at_offset = GetTextAtOffset;
  iface->get_caret_offset = GetCaretOffset;
  iface->set_caret_offset = SetCaretOffset;
  iface->get_character_extents = GetCharacterExtents;
  iface->get_range_extents = GetRangeExtents;
  iface->get_n_selections = GetNSelections;
  iface->get_selection = GetSelection;
  iface->add_selection = AddSelection;
  iface->remove_selection = RemoveSelection;
  iface->set_selection = SetSelection;

  iface->get_run_attributes = GetRunAttributes;
  iface->get_default_attributes = GetDefaultAttributes;

#if defined(ATK_210)
  iface->get_string_at_offset = GetStringAtOffset;
#endif
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_text

namespace atk_window {
void Init(AtkWindowIface* iface) {}
const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};
}  // namespace atk_window

namespace atk_selection {

gboolean AddSelection(AtkSelection* selection, gint index) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(selection));
  if (!obj)
    return FALSE;
  if (index < 0 || index >= obj->GetChildCount())
    return FALSE;

  AXPlatformNodeAuraLinux* child =
      AtkObjectToAXPlatformNodeAuraLinux(obj->ChildAtIndex(index));
  if (!child)
    return FALSE;

  if (!child->SupportsSelectionWithAtkSelection())
    return FALSE;

  bool selected = child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
  if (selected)
    return TRUE;

  AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  return child->GetDelegate()->AccessibilityPerformAction(data);
}

gboolean ClearSelection(AtkSelection* selection) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(selection));
  if (!obj)
    return FALSE;

  int child_count = obj->GetChildCount();
  bool success = true;
  for (int i = 0; i < child_count; ++i) {
    AXPlatformNodeAuraLinux* child =
        AtkObjectToAXPlatformNodeAuraLinux(obj->ChildAtIndex(i));
    if (!child)
      continue;

    if (!child->SupportsSelectionWithAtkSelection())
      continue;

    bool selected =
        child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
    if (!selected)
      continue;

    AXActionData data;
    data.action = ax::mojom::Action::kDoDefault;
    success = success && child->GetDelegate()->AccessibilityPerformAction(data);
  }

  return success;
}

AtkObject* RefSelection(AtkSelection* selection, gint requested_child_index) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(selection));
  if (!obj)
    return nullptr;

  int child_count = obj->GetChildCount();
  gint selected_count = 0;
  for (int i = 0; i < child_count; ++i) {
    AtkObject* child = obj->ChildAtIndex(i);
    AXPlatformNodeAuraLinux* child_ax_node =
        AtkObjectToAXPlatformNodeAuraLinux(child);
    if (!child_ax_node)
      continue;

    if (child_ax_node->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
      if (selected_count == requested_child_index)
        return static_cast<AtkObject*>(g_object_ref(child));
      ++selected_count;
    }
  }

  return nullptr;
}

gint GetSelectionCount(AtkSelection* selection) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(selection));
  if (!obj)
    return 0;

  int child_count = obj->GetChildCount();
  gint selected_count = 0;
  for (int i = 0; i < child_count; ++i) {
    AXPlatformNodeAuraLinux* child =
        AtkObjectToAXPlatformNodeAuraLinux(obj->ChildAtIndex(i));
    if (!child)
      continue;

    if (child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
      ++selected_count;
  }

  return selected_count;
}

gboolean IsChildSelected(AtkSelection* selection, gint index) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(selection));
  if (!obj)
    return FALSE;
  if (index < 0 || index >= obj->GetChildCount())
    return FALSE;

  AXPlatformNodeAuraLinux* child =
      AtkObjectToAXPlatformNodeAuraLinux(obj->ChildAtIndex(index));
  return child && child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
}

gboolean RemoveSelection(AtkSelection* selection,
                         gint index_into_selected_children) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(selection));
  if (!obj)
    return FALSE;

  int child_count = obj->GetChildCount();
  for (int i = 0; i < child_count; ++i) {
    AXPlatformNodeAuraLinux* child =
        AtkObjectToAXPlatformNodeAuraLinux(obj->ChildAtIndex(i));
    if (!child)
      continue;

    bool selected =
        child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
    if (selected && index_into_selected_children == 0) {
      if (!child->SupportsSelectionWithAtkSelection())
        return FALSE;

      AXActionData data;
      data.action = ax::mojom::Action::kDoDefault;
      return child->GetDelegate()->AccessibilityPerformAction(data);
    } else if (selected) {
      index_into_selected_children--;
    }
  }

  return FALSE;
}

gboolean SelectAllSelection(AtkSelection* selection) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(selection));
  if (!obj)
    return FALSE;

  int child_count = obj->GetChildCount();
  bool success = true;
  for (int i = 0; i < child_count; ++i) {
    AXPlatformNodeAuraLinux* child =
        AtkObjectToAXPlatformNodeAuraLinux(obj->ChildAtIndex(i));
    if (!child)
      continue;

    if (!child->SupportsSelectionWithAtkSelection())
      continue;

    bool selected =
        child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
    if (selected)
      continue;

    AXActionData data;
    data.action = ax::mojom::Action::kDoDefault;
    success = success && child->GetDelegate()->AccessibilityPerformAction(data);
  }

  return success;
}

void Init(AtkSelectionIface* iface) {
  iface->add_selection = AddSelection;
  iface->clear_selection = ClearSelection;
  iface->ref_selection = RefSelection;
  iface->get_selection_count = GetSelectionCount;
  iface->is_child_selected = IsChildSelected;
  iface->remove_selection = RemoveSelection;
  iface->select_all_selection = SelectAllSelection;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_selection

namespace atk_table {

AtkObject* RefAt(AtkTable* table, gint row, gint column) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    if (AXPlatformNodeBase* cell = obj->GetTableCell(row, column)) {
      if (AtkObject* atk_cell = cell->GetNativeViewAccessible()) {
        g_object_ref(atk_cell);
        return atk_cell;
      }
    }
  }

  return nullptr;
}

gint GetIndexAt(AtkTable* table, gint row, gint column) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    if (const AXPlatformNodeBase* cell = obj->GetTableCell(row, column)) {
      DCHECK(cell->GetTableCellIndex().has_value());
      return cell->GetTableCellIndex().value();
    }
  }

  return -1;
}

gint GetColumnAtIndex(AtkTable* table, gint index) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    if (const AXPlatformNodeBase* cell = obj->GetTableCell(index)) {
      DCHECK(cell->GetTableColumn().has_value());
      return cell->GetTableColumn().value();
    }
  }

  return -1;
}

gint GetRowAtIndex(AtkTable* table, gint index) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    if (const AXPlatformNodeBase* cell = obj->GetTableCell(index)) {
      DCHECK(cell->GetTableRow().has_value());
      return cell->GetTableRow().value();
    }
  }

  return -1;
}

gint GetNColumns(AtkTable* table) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    // If the object is not a table, we return 0.
    return obj->GetTableColumnCount().value_or(0);
  }

  return 0;
}

gint GetNRows(AtkTable* table) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    // If the object is not a table, we return 0.
    return obj->GetTableRowCount().value_or(0);
  }

  return 0;
}

gint GetColumnExtentAt(AtkTable* table, gint row, gint column) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    if (const AXPlatformNodeBase* cell = obj->GetTableCell(row, column)) {
      DCHECK(cell->GetTableColumnSpan().has_value());
      return cell->GetTableColumnSpan().value();
    }
  }

  return 0;
}

gint GetRowExtentAt(AtkTable* table, gint row, gint column) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    if (const AXPlatformNodeBase* cell = obj->GetTableCell(row, column)) {
      DCHECK(cell->GetTableRowSpan().has_value());
      return cell->GetTableRowSpan().value();
    }
  }

  return 0;
}

AtkObject* GetColumnHeader(AtkTable* table, gint column) {
  auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table));
  if (!obj)
    return nullptr;

  // AtkTable supports only one column header object. So return the first one
  // we find. In the case of multiple headers, ATs can fall back on the column
  // description.
  std::vector<int32_t> ids = obj->GetDelegate()->GetColHeaderNodeIds(column);
  for (const auto& node_id : ids) {
    if (AXPlatformNode* header = obj->GetDelegate()->GetFromNodeID(node_id)) {
      if (AtkObject* atk_header = header->GetNativeViewAccessible()) {
        g_object_ref(atk_header);
        return atk_header;
      }
    }
  }

  return nullptr;
}

AtkObject* GetRowHeader(AtkTable* table, gint row) {
  auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table));
  if (!obj)
    return nullptr;

  // AtkTable supports only one row header object. So return the first one
  // we find. In the case of multiple headers, ATs can fall back on the row
  // description.
  std::vector<int32_t> ids = obj->GetDelegate()->GetRowHeaderNodeIds(row);
  for (const auto& node_id : ids) {
    if (AXPlatformNode* header = obj->GetDelegate()->GetFromNodeID(node_id)) {
      if (AtkObject* atk_header = header->GetNativeViewAccessible()) {
        g_object_ref(atk_header);
        return atk_header;
      }
    }
  }

  return nullptr;
}

AtkObject* GetCaption(AtkTable* table) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    if (auto* caption = obj->GetTableCaption())
      return caption->GetNativeViewAccessible();
  }

  return nullptr;
}

const gchar* GetColumnDescription(AtkTable* table, gint column) {
  auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table));
  if (!obj)
    return nullptr;

  std::vector<int32_t> ids = obj->GetDelegate()->GetColHeaderNodeIds(column);
  return BuildDescriptionFromHeaders(obj->GetDelegate(), ids);
}

const gchar* GetRowDescription(AtkTable* table, gint row) {
  auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table));
  if (!obj)
    return nullptr;

  std::vector<int32_t> ids = obj->GetDelegate()->GetRowHeaderNodeIds(row);
  return BuildDescriptionFromHeaders(obj->GetDelegate(), ids);
}

void Init(AtkTableIface* iface) {
  iface->ref_at = RefAt;
  iface->get_index_at = GetIndexAt;
  iface->get_column_at_index = GetColumnAtIndex;
  iface->get_row_at_index = GetRowAtIndex;
  iface->get_n_columns = GetNColumns;
  iface->get_n_rows = GetNRows;
  iface->get_column_extent_at = GetColumnExtentAt;
  iface->get_row_extent_at = GetRowExtentAt;
  iface->get_column_header = GetColumnHeader;
  iface->get_row_header = GetRowHeader;
  iface->get_caption = GetCaption;
  iface->get_column_description = GetColumnDescription;
  iface->get_row_description = GetRowDescription;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_table

// The ATK table cell interface was added in ATK 2.12.
#if defined(ATK_212)

namespace atk_table_cell {

gint GetColumnSpan(AtkTableCell* cell) {
  if (const AXPlatformNodeBase* obj =
          AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(cell))) {
    // If the object is not a cell, we return 0.
    return obj->GetTableColumnSpan().value_or(0);
  }

  return 0;
}

GPtrArray* GetColumnHeaderCells(AtkTableCell* cell) {
  GPtrArray* array = g_ptr_array_new_with_free_func(g_object_unref);

  // AtkTableCell is implemented on cells, row headers, and column headers.
  // Calling GetColHeaderNodeIds() on a column header cell will include that
  // column header, along with any other column headers in the column which
  // may or may not describe the header cell in question. Therefore, just return
  // headers for non-header cells.
  if (atk_object_get_role(ATK_OBJECT(cell)) != ATK_ROLE_TABLE_CELL)
    return array;

  auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(cell));
  if (!obj)
    return array;

  base::Optional<int> col_index = obj->GetTableColumn();
  if (!col_index)
    return array;

  const std::vector<int32_t> ids =
      obj->GetDelegate()->GetColHeaderNodeIds(*col_index);
  for (const auto& node_id : ids) {
    if (AXPlatformNode* node = obj->GetDelegate()->GetFromNodeID(node_id)) {
      if (AtkObject* atk_node = node->GetNativeViewAccessible()) {
        g_ptr_array_add(array, g_object_ref(atk_node));
      }
    }
  }

  return array;
}

gboolean GetCellPosition(AtkTableCell* cell, gint* row, gint* column) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(cell))) {
    base::Optional<int> row_index = obj->GetTableRow();
    base::Optional<int> col_index = obj->GetTableColumn();
    if (!row_index || !col_index)
      return false;

    *row = *row_index;
    *column = *col_index;
    return true;
  }

  return false;
}

gint GetRowSpan(AtkTableCell* cell) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(cell))) {
    // If the object is not a cell, we return 0.
    return obj->GetTableRowSpan().value_or(0);
  }

  return 0;
}

GPtrArray* GetRowHeaderCells(AtkTableCell* cell) {
  GPtrArray* array = g_ptr_array_new_with_free_func(g_object_unref);

  // AtkTableCell is implemented on cells, row headers, and column headers.
  // Calling GetRowHeaderNodeIds() on a row header cell will include that
  // row header, along with any other row headers in the row which may or
  // may not describe the header cell in question. Therefore, just return
  // headers for non-header cells.
  if (atk_object_get_role(ATK_OBJECT(cell)) != ATK_ROLE_TABLE_CELL)
    return array;

  auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(cell));
  if (!obj)
    return array;

  base::Optional<int> row_index = obj->GetTableRow();
  if (!row_index)
    return array;

  const std::vector<int32_t> ids =
      obj->GetDelegate()->GetRowHeaderNodeIds(*row_index);
  for (const auto& node_id : ids) {
    if (AXPlatformNode* node = obj->GetDelegate()->GetFromNodeID(node_id)) {
      if (AtkObject* atk_node = node->GetNativeViewAccessible()) {
        g_ptr_array_add(array, g_object_ref(atk_node));
      }
    }
  }

  return array;
}

AtkObject* GetTable(AtkTableCell* cell) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(cell))) {
    if (auto* table = obj->GetTable())
      return table->GetNativeViewAccessible();
  }

  return nullptr;
}

void Init(AtkTableCellIface* iface) {
  iface->get_column_span = GetColumnSpan;
  iface->get_column_header_cells = GetColumnHeaderCells;
  iface->get_position = GetCellPosition;
  iface->get_row_span = GetRowSpan;
  iface->get_row_header_cells = GetRowHeaderCells;
  iface->get_table = GetTable;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_table_cell

#endif  // ATK_212

namespace atk_object {

gpointer kAXPlatformNodeAuraLinuxParentClass = nullptr;

const gchar* GetName(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  ax::mojom::NameFrom name_from = obj->GetData().GetNameFrom();
  if (obj->GetStringAttribute(ax::mojom::StringAttribute::kName).empty() &&
      name_from != ax::mojom::NameFrom::kAttributeExplicitlyEmpty)
    return nullptr;

  obj->accessible_name_ =
      obj->GetStringAttribute(ax::mojom::StringAttribute::kName);
  return obj->accessible_name_.c_str();
}

const gchar* GetDescription(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetStringAttribute(ax::mojom::StringAttribute::kDescription)
      .c_str();
}

gint GetIndexInParent(AtkObject* atk_object) {
  AtkObject* parent = atk_object_get_parent(atk_object);
  if (!parent)
    return -1;

  int n_children = atk_object_get_n_accessible_children(parent);
  for (int i = 0; i < n_children; i++) {
    AtkObject* child = atk_object_ref_accessible_child(parent, i);
    g_object_unref(child);
    if (child == atk_object)
      return i;
  }

  return -1;
}

AtkObject* GetParent(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetParent();
}

gint GetNChildren(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  return obj->GetChildCount();
}

AtkObject* RefChild(AtkObject* atk_object, gint index) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  if (index < 0 || index >= obj->GetChildCount())
    return nullptr;

  AtkObject* result = obj->ChildAtIndex(index);
  if (result)
    g_object_ref(result);
  return result;
}

AtkRelationSet* RefRelationSet(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return atk_relation_set_new();
  return obj->GetAtkRelations();
}

AtkAttributeSet* GetAttributes(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetAtkAttributes();
}

AtkRole GetRole(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return ATK_ROLE_INVALID;
  return obj->GetAtkRole();
}

AtkStateSet* RefStateSet(AtkObject* atk_object) {
  AtkStateSet* atk_state_set =
      ATK_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)
          ->ref_state_set(atk_object);

  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_DEFUNCT);
  } else {
    obj->GetAtkState(atk_state_set);
  }
  return atk_state_set;
}
void Initialize(AtkObject* atk_object, gpointer data) {
  if (ATK_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)->initialize) {
    ATK_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)
        ->initialize(atk_object, data);
  }

  AX_PLATFORM_NODE_AURALINUX(atk_object)->m_object =
      reinterpret_cast<AXPlatformNodeAuraLinux*>(data);
}

void Finalize(GObject* atk_object) {
  G_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)->finalize(atk_object);
}

void ClassInit(gpointer class_pointer, gpointer /* class_data */) {
  GObjectClass* gobject_class = G_OBJECT_CLASS(class_pointer);
  kAXPlatformNodeAuraLinuxParentClass = g_type_class_peek_parent(gobject_class);
  gobject_class->finalize = Finalize;

  AtkObjectClass* atk_object_class = ATK_OBJECT_CLASS(gobject_class);
  atk_object_class->initialize = Initialize;
  atk_object_class->get_name = GetName;
  atk_object_class->get_description = GetDescription;
  atk_object_class->get_parent = GetParent;
  atk_object_class->get_n_children = GetNChildren;
  atk_object_class->ref_child = RefChild;
  atk_object_class->get_role = GetRole;
  atk_object_class->ref_state_set = RefStateSet;
  atk_object_class->get_index_in_parent = GetIndexInParent;
  atk_object_class->ref_relation_set = RefRelationSet;
  atk_object_class->get_attributes = GetAttributes;
}

GType GetType() {
  AXPlatformNodeAuraLinux::EnsureGTypeInit();

  static volatile gsize type_volatile = 0;
  if (g_once_init_enter(&type_volatile)) {
    static const GTypeInfo type_info = {
        sizeof(AXPlatformNodeAuraLinuxClass),  // class_size
        nullptr,                               // base_init
        nullptr,                               // base_finalize
        atk_object::ClassInit,
        nullptr,                                // class_finalize
        nullptr,                                // class_data
        sizeof(AXPlatformNodeAuraLinuxObject),  // instance_size
        0,                                      // n_preallocs
        nullptr,                                // instance_init
        nullptr                                 // value_table
    };

    GType type = g_type_register_static(
        ATK_TYPE_OBJECT, "AXPlatformNodeAuraLinux", &type_info, GTypeFlags(0));
    g_once_init_leave(&type_volatile, type);
  }

  return type_volatile;
}

void Detach(AXPlatformNodeAuraLinuxObject* atk_object) {
  if (!atk_object->m_object)
    return;

  atk_object->m_object = nullptr;
  atk_object_notify_state_change(ATK_OBJECT(atk_object), ATK_STATE_DEFUNCT,
                                 TRUE);
}

}  //  namespace atk_object

static PROTECTED_MEMORY_SECTION
    base::ProtectedMemory<AtkTableCellInterface::GetTypeFunc>
        g_atk_table_cell_get_type;

static PROTECTED_MEMORY_SECTION
    base::ProtectedMemory<AtkTableCellInterface::GetColumnHeaderCellsFunc>
        g_atk_table_cell_get_column_header_cells;

static PROTECTED_MEMORY_SECTION
    base::ProtectedMemory<AtkTableCellInterface::GetRowHeaderCellsFunc>
        g_atk_table_cell_get_row_header_cells;

static PROTECTED_MEMORY_SECTION
    base::ProtectedMemory<AtkTableCellInterface::GetRowColumnSpanFunc>
        g_atk_table_cell_get_row_column_span;

}  // namespace

// static
base::Optional<AtkTableCellInterface> AtkTableCellInterface::Get() {
  static base::Optional<AtkTableCellInterface> interface = base::nullopt;
  static base::ProtectedMemory<GetTypeFunc>::Initializer
      init_atk_table_cell_get_type(
          &g_atk_table_cell_get_type,
          reinterpret_cast<GetTypeFunc>(
              dlsym(RTLD_DEFAULT, "atk_table_cell_get_type")));
  static base::ProtectedMemory<GetColumnHeaderCellsFunc>::Initializer
      init_atk_table_cell_get_column_header_cells(
          &g_atk_table_cell_get_column_header_cells,
          reinterpret_cast<GetColumnHeaderCellsFunc>(
              dlsym(RTLD_DEFAULT, "atk_table_cell_get_column_header_cells")));
  static base::ProtectedMemory<GetRowHeaderCellsFunc>::Initializer
      init_atk_table_cell_get_row_header_cells(
          &g_atk_table_cell_get_row_header_cells,
          reinterpret_cast<GetRowHeaderCellsFunc>(
              dlsym(RTLD_DEFAULT, "atk_table_cell_get_row_header_cells")));
  static base::ProtectedMemory<GetRowColumnSpanFunc>::Initializer
      init_atk_table_cell_get_row_column_span(
          &g_atk_table_cell_get_row_column_span,
          reinterpret_cast<GetRowColumnSpanFunc>(
              dlsym(RTLD_DEFAULT, "atk_table_cell_get_row_column_span")));

  if (interface.has_value())
    return **interface->GetType ? interface : base::nullopt;

  interface.emplace();
  interface->GetType = &g_atk_table_cell_get_type;
  interface->GetColumnHeaderCells = &g_atk_table_cell_get_column_header_cells;
  interface->GetRowHeaderCells = &g_atk_table_cell_get_row_header_cells;
  interface->GetRowColumnSpan = &g_atk_table_cell_get_row_column_span;
  interface->initialized = true;
  return **interface->GetType ? interface : base::nullopt;
}

void AXPlatformNodeAuraLinux::EnsureGTypeInit() {
#if !GLIB_CHECK_VERSION(2, 36, 0)
  static bool first_time = true;
  if (UNLIKELY(first_time)) {
    g_type_init();
    first_time = false;
  }
#endif
}

int AXPlatformNodeAuraLinux::GetGTypeInterfaceMask() {
  int interface_mask = 0;

  // Component interface is always supported.
  interface_mask |= 1 << ATK_COMPONENT_INTERFACE;

  // Action interface is basic one. It just relays on executing default action
  // for each object.
  interface_mask |= 1 << ATK_ACTION_INTERFACE;

  // TODO(accessibility): We should only expose this for some elements, but
  // it might be better to do this after exposing the hypertext interface
  // as well.
  interface_mask |= 1 << ATK_TEXT_INTERFACE;

  if (!IsPlainTextField() && !IsChildOfLeaf())
    interface_mask |= 1 << ATK_HYPERTEXT_INTERFACE;

  // Value Interface
  AtkRole role = GetAtkRole();
  if (IsRangeValueSupported(GetData())) {
    interface_mask |= 1 << ATK_VALUE_INTERFACE;
  }

  // Document Interface
  if (role == ATK_ROLE_DOCUMENT_WEB)
    interface_mask |= 1 << ATK_DOCUMENT_INTERFACE;

  // Image Interface
  if (role == ATK_ROLE_IMAGE || role == ATK_ROLE_IMAGE_MAP)
    interface_mask |= 1 << ATK_IMAGE_INTERFACE;

  // The AtkHyperlinkImpl interface allows getting a AtkHyperlink from an
  // AtkObject. It is indeed implemented by actual web hyperlinks, but also by
  // objects that will become embedded objects in ATK hypertext, so the name is
  // a bit of a misnomer from the ATK API.
  if (role == ATK_ROLE_LINK || (!IsTextOnlyObject() && GetParent())) {
    interface_mask |= 1 << ATK_HYPERLINK_INTERFACE;
  }

  if (role == ATK_ROLE_FRAME)
    interface_mask |= 1 << ATK_WINDOW_INTERFACE;

  if (IsContainerWithSelectableChildren(GetData().role))
    interface_mask |= 1 << ATK_SELECTION_INTERFACE;

  if (role == ATK_ROLE_TABLE || role == ATK_ROLE_TREE_TABLE)
    interface_mask |= 1 << ATK_TABLE_INTERFACE;

  // Because the TableCell Interface is only supported in ATK version 2.12 and
  // later, GetAccessibilityGType has a runtime check to verify we have a recent
  // enough version. If we don't, GetAccessibilityGType will exclude
  // AtkTableCell from the supported interfaces and none of its methods or
  // properties will be exposed to assistive technologies.
  if (role == ATK_ROLE_TABLE_CELL || role == ATK_ROLE_COLUMN_HEADER ||
      role == ATK_ROLE_ROW_HEADER)
    interface_mask |= 1 << ATK_TABLE_CELL_INTERFACE;

  return interface_mask;
}

GType AXPlatformNodeAuraLinux::GetAccessibilityGType() {
  static const GTypeInfo type_info = {
      sizeof(AXPlatformNodeAuraLinuxClass),
      (GBaseInitFunc) nullptr,
      (GBaseFinalizeFunc) nullptr,
      (GClassInitFunc) nullptr,
      (GClassFinalizeFunc) nullptr,
      nullptr,                               /* class data */
      sizeof(AXPlatformNodeAuraLinuxObject), /* instance size */
      0,                                     /* nb preallocs */
      (GInstanceInitFunc) nullptr,
      nullptr /* value table */
  };

  const char* atk_type_name = GetUniqueAccessibilityGTypeName(interface_mask_);
  GType type = g_type_from_name(atk_type_name);
  if (type)
    return type;

  type = g_type_register_static(AX_PLATFORM_NODE_AURALINUX_TYPE, atk_type_name,
                                &type_info, GTypeFlags(0));
  if (interface_mask_ & (1 << ATK_COMPONENT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_COMPONENT, &atk_component::Info);
  if (interface_mask_ & (1 << ATK_ACTION_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_ACTION, &atk_action::Info);
  if (interface_mask_ & (1 << ATK_DOCUMENT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_DOCUMENT, &atk_document::Info);
  if (interface_mask_ & (1 << ATK_IMAGE_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_IMAGE, &atk_image::Info);
  if (interface_mask_ & (1 << ATK_VALUE_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_VALUE, &atk_value::Info);
  if (interface_mask_ & (1 << ATK_HYPERLINK_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_HYPERLINK_IMPL,
                                &atk_hyperlink::Info);
  if (interface_mask_ & (1 << ATK_HYPERTEXT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_HYPERTEXT, &atk_hypertext::Info);
  if (interface_mask_ & (1 << ATK_TEXT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_TEXT, &atk_text::Info);
  if (interface_mask_ & (1 << ATK_WINDOW_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_WINDOW, &atk_window::Info);
  if (interface_mask_ & (1 << ATK_SELECTION_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_SELECTION, &atk_selection::Info);
  if (interface_mask_ & (1 << ATK_TABLE_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_TABLE, &atk_table::Info);

  if (interface_mask_ & (1 << ATK_TABLE_CELL_INTERFACE)) {
    // Run-time check to ensure AtkTableCell is supported (requires ATK 2.12).
    auto interface = AtkTableCellInterface::Get();
    if (interface.has_value()) {
      g_type_add_interface_static(
          type, base::UnsanitizedCfiCall(*interface->GetType)(),
          &atk_table_cell::Info);
    }
  }

  return type;
}

AtkObject* AXPlatformNodeAuraLinux::CreateAtkObject() {
  EnsureGTypeInit();
  interface_mask_ = GetGTypeInterfaceMask();
  GType type = GetAccessibilityGType();
  AtkObject* atk_object = static_cast<AtkObject*>(g_object_new(type, nullptr));

  atk_object_initialize(atk_object, this);

  return ATK_OBJECT(atk_object);
}

void AXPlatformNodeAuraLinux::DestroyAtkObjects() {
  if (atk_hyperlink_) {
    ax_platform_atk_hyperlink_set_object(
        AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink_), nullptr);
    g_object_unref(atk_hyperlink_);
    atk_hyperlink_ = nullptr;
  }

  if (atk_object_) {
    // We explicitly clear g_current_focused and g_current_active_descendant
    // just in case there is another reference to atk_object_ somewhere.
    if (atk_object_ == g_current_focused)
      SetWeakGPtrToAtkObject(&g_current_focused, nullptr);
    if (atk_object_ == g_current_active_descendant)
      SetWeakGPtrToAtkObject(&g_current_active_descendant, nullptr);
    atk_object::Detach(AX_PLATFORM_NODE_AURALINUX(atk_object_));

    g_object_unref(atk_object_);
    atk_object_ = nullptr;
  }
}

// static
AXPlatformNode* AXPlatformNode::Create(AXPlatformNodeDelegate* delegate) {
  AXPlatformNodeAuraLinux* node = new AXPlatformNodeAuraLinux();
  node->Init(delegate);
  return node;
}

// static
AXPlatformNode* AXPlatformNode::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  return AtkObjectToAXPlatformNodeAuraLinux(accessible);
}

//
// AXPlatformNodeAuraLinux implementation.
//

// static
void AXPlatformNodeAuraLinux::SetApplication(AXPlatformNode* application) {
  g_root_application = application;
}

// static
AXPlatformNode* AXPlatformNodeAuraLinux::application() {
  return g_root_application;
}

// static
void AXPlatformNodeAuraLinux::StaticInitialize() {
  AtkUtilAuraLinux::GetInstance()->InitializeAsync();
}

AtkRole AXPlatformNodeAuraLinux::GetAtkRole() {
  switch (GetData().role) {
    case ax::mojom::Role::kAlert:
      return ATK_ROLE_ALERT;
    case ax::mojom::Role::kAlertDialog:
      return ATK_ROLE_DIALOG;
    case ax::mojom::Role::kAnchor:
      return ATK_ROLE_LINK;
    case ax::mojom::Role::kAnnotation:
      // TODO(accessibility) Panels are generally for containers of widgets.
      // This should probably be a section (if a container) or static if text.
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kApplication:
      // Only use ATK_ROLE_APPLICATION for elements with no parent, since it
      // is only for top level app windows and not ARIA applications.
      if (!GetParent()) {
        return ATK_ROLE_APPLICATION;
      } else {
        return ATK_ROLE_EMBEDDED;
      }
    case ax::mojom::Role::kArticle:
      return ATK_ROLE_ARTICLE;
    case ax::mojom::Role::kAudio:
      return ATK_ROLE_AUDIO;
    case ax::mojom::Role::kBanner:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kBlockquote:
      return ATK_ROLE_BLOCK_QUOTE;
    case ax::mojom::Role::kCaret:
      return ATK_ROLE_UNKNOWN;
    case ax::mojom::Role::kButton:
      return ATK_ROLE_PUSH_BUTTON;
    case ax::mojom::Role::kCanvas:
      return ATK_ROLE_CANVAS;
    case ax::mojom::Role::kCaption:
      return ATK_ROLE_CAPTION;
    case ax::mojom::Role::kCell:
      return ATK_ROLE_TABLE_CELL;
    case ax::mojom::Role::kCheckBox:
      return ATK_ROLE_CHECK_BOX;
    case ax::mojom::Role::kSwitch:
      return ATK_ROLE_TOGGLE_BUTTON;
    case ax::mojom::Role::kColorWell:
      return ATK_ROLE_PUSH_BUTTON;
    case ax::mojom::Role::kColumn:
      return ATK_ROLE_UNKNOWN;
    case ax::mojom::Role::kColumnHeader:
      return ATK_ROLE_COLUMN_HEADER;
    case ax::mojom::Role::kComboBoxGrouping:
      return ATK_ROLE_COMBO_BOX;
    case ax::mojom::Role::kComboBoxMenuButton:
      return ATK_ROLE_COMBO_BOX;
    case ax::mojom::Role::kComplementary:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kContentDeletion:
    case ax::mojom::Role::kContentInsertion:
      // TODO(accessibility) https://github.com/w3c/html-aam/issues/141
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kDate:
      return ATK_ROLE_DATE_EDITOR;
    case ax::mojom::Role::kDateTime:
      return ATK_ROLE_DATE_EDITOR;
    case ax::mojom::Role::kDefinition:
    case ax::mojom::Role::kDescriptionListDetail:
      return ATK_ROLE_DESCRIPTION_VALUE;
    case ax::mojom::Role::kDescriptionList:
      return ATK_ROLE_DESCRIPTION_LIST;
    case ax::mojom::Role::kDescriptionListTerm:
      return ATK_ROLE_DESCRIPTION_TERM;
    case ax::mojom::Role::kDetails:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kDialog:
      return ATK_ROLE_DIALOG;
    case ax::mojom::Role::kDirectory:
      return ATK_ROLE_LIST;
    case ax::mojom::Role::kDisclosureTriangle:
      return ATK_ROLE_TOGGLE_BUTTON;
    case ax::mojom::Role::kDocCover:
      return ATK_ROLE_IMAGE;
    case ax::mojom::Role::kDocBackLink:
    case ax::mojom::Role::kDocBiblioRef:
    case ax::mojom::Role::kDocGlossRef:
    case ax::mojom::Role::kDocNoteRef:
      return ATK_ROLE_LINK;
    case ax::mojom::Role::kDocBiblioEntry:
    case ax::mojom::Role::kDocEndnote:
      return ATK_ROLE_LIST_ITEM;
    case ax::mojom::Role::kDocNotice:
    case ax::mojom::Role::kDocTip:
      return ATK_ROLE_COMMENT;
    case ax::mojom::Role::kDocFootnote:
      return kAtkFootnoteRole;
    case ax::mojom::Role::kDocPageBreak:
      return ATK_ROLE_SEPARATOR;
    case ax::mojom::Role::kDocAcknowledgments:
    case ax::mojom::Role::kDocAfterword:
    case ax::mojom::Role::kDocAppendix:
    case ax::mojom::Role::kDocBibliography:
    case ax::mojom::Role::kDocChapter:
    case ax::mojom::Role::kDocConclusion:
    case ax::mojom::Role::kDocCredits:
    case ax::mojom::Role::kDocEndnotes:
    case ax::mojom::Role::kDocEpilogue:
    case ax::mojom::Role::kDocErrata:
    case ax::mojom::Role::kDocForeword:
    case ax::mojom::Role::kDocGlossary:
    case ax::mojom::Role::kDocIndex:
    case ax::mojom::Role::kDocIntroduction:
    case ax::mojom::Role::kDocPageList:
    case ax::mojom::Role::kDocPart:
    case ax::mojom::Role::kDocPreface:
    case ax::mojom::Role::kDocPrologue:
    case ax::mojom::Role::kDocToc:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kDocAbstract:
    case ax::mojom::Role::kDocColophon:
    case ax::mojom::Role::kDocCredit:
    case ax::mojom::Role::kDocDedication:
    case ax::mojom::Role::kDocEpigraph:
    case ax::mojom::Role::kDocExample:
    case ax::mojom::Role::kDocPullquote:
    case ax::mojom::Role::kDocQna:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kDocSubtitle:
      return ATK_ROLE_HEADING;
    case ax::mojom::Role::kDocument:
      return ATK_ROLE_DOCUMENT_FRAME;
    case ax::mojom::Role::kEmbeddedObject:
      return ATK_ROLE_EMBEDDED;
    case ax::mojom::Role::kForm:
      // TODO(accessibility) Forms which lack an accessible name are no longer
      // exposed as forms. http://crbug.com/874384. Forms which have accessible
      // names should be exposed as ATK_ROLE_LANDMARK according to Core AAM.
      return ATK_ROLE_FORM;
    case ax::mojom::Role::kFigure:
    case ax::mojom::Role::kFeed:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kGenericContainer:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kGraphicsDocument:
      return ATK_ROLE_DOCUMENT_FRAME;
    case ax::mojom::Role::kGraphicsObject:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kGraphicsSymbol:
      return ATK_ROLE_IMAGE;
    case ax::mojom::Role::kGrid:
      return ATK_ROLE_TABLE;
    case ax::mojom::Role::kGroup:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kHeading:
      return ATK_ROLE_HEADING;
    case ax::mojom::Role::kIframe:
    case ax::mojom::Role::kIframePresentational:
      return ATK_ROLE_INTERNAL_FRAME;
    case ax::mojom::Role::kIgnored:
      return ATK_ROLE_REDUNDANT_OBJECT;
    case ax::mojom::Role::kImage:
      return ATK_ROLE_IMAGE;
    case ax::mojom::Role::kImageMap:
      return ATK_ROLE_IMAGE_MAP;
    case ax::mojom::Role::kInlineTextBox:
      // TODO(jdiggs) This should be ATK_ROLE_STATIC. https://crbug.com/984590
      return ATK_ROLE_TEXT;
    case ax::mojom::Role::kInputTime:
      return ATK_ROLE_DATE_EDITOR;
    case ax::mojom::Role::kLabelText:
      return ATK_ROLE_LABEL;
    case ax::mojom::Role::kLegend:
      return ATK_ROLE_LABEL;
    // Layout table objects are treated the same as Role::kGenericContainer.
    case ax::mojom::Role::kLayoutTable:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kLayoutTableCell:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kLayoutTableColumn:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kLayoutTableRow:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kLineBreak:
      // TODO(Accessibility) Having a separate accessible object for line breaks
      // is inconsistent with other implementations. http://crbug.com/873144#c1.
      // TODO(jdiggs) This should be ATK_ROLE_STATIC. https://crbug.com/984590
      return ATK_ROLE_TEXT;
    case ax::mojom::Role::kLink:
      return ATK_ROLE_LINK;
    case ax::mojom::Role::kList:
      return ATK_ROLE_LIST;
    case ax::mojom::Role::kListBox:
      return ATK_ROLE_LIST_BOX;
    // TODO(Accessibility) Use ATK_ROLE_MENU_ITEM inside a combo box, see how
    // ax_platform_node_win.cc code does this.
    case ax::mojom::Role::kListBoxOption:
      return ATK_ROLE_LIST_ITEM;
    case ax::mojom::Role::kListGrid:
      return ATK_ROLE_TABLE;
    case ax::mojom::Role::kListItem:
      return ATK_ROLE_LIST_ITEM;
    case ax::mojom::Role::kListMarker:
      // TODO(Accessibility) Having a separate accessible object for the marker
      // is inconsistent with other implementations. http://crbug.com/873144.
      return kStaticRole;
    case ax::mojom::Role::kLog:
      return ATK_ROLE_LOG;
    case ax::mojom::Role::kMain:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kMark:
      return kStaticRole;
    case ax::mojom::Role::kMath:
      return ATK_ROLE_MATH;
    case ax::mojom::Role::kMarquee:
      return ATK_ROLE_MARQUEE;
    case ax::mojom::Role::kMenu:
      return ATK_ROLE_MENU;
    case ax::mojom::Role::kMenuButton:
      return ATK_ROLE_MENU;
    case ax::mojom::Role::kMenuBar:
      return ATK_ROLE_MENU_BAR;
    case ax::mojom::Role::kMenuItem:
      return ATK_ROLE_MENU_ITEM;
    case ax::mojom::Role::kMenuItemCheckBox:
      return ATK_ROLE_CHECK_MENU_ITEM;
    case ax::mojom::Role::kMenuItemRadio:
      return ATK_ROLE_RADIO_MENU_ITEM;
    case ax::mojom::Role::kMenuListPopup:
      return ATK_ROLE_MENU;
    case ax::mojom::Role::kMenuListOption:
      return ATK_ROLE_MENU_ITEM;
    case ax::mojom::Role::kMeter:
      return ATK_ROLE_LEVEL_BAR;
    case ax::mojom::Role::kNavigation:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kNote:
      return ATK_ROLE_COMMENT;
    case ax::mojom::Role::kPane:
    case ax::mojom::Role::kScrollView:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kParagraph:
      return ATK_ROLE_PARAGRAPH;
    case ax::mojom::Role::kPopUpButton: {
      std::string html_tag =
          GetData().GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
      if (html_tag == "select")
        return ATK_ROLE_COMBO_BOX;
      return ATK_ROLE_PUSH_BUTTON;
    }
    case ax::mojom::Role::kPre:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kProgressIndicator:
      return ATK_ROLE_PROGRESS_BAR;
    case ax::mojom::Role::kRadioButton:
      return ATK_ROLE_RADIO_BUTTON;
    case ax::mojom::Role::kRadioGroup:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kRegion: {
      std::string html_tag =
          GetData().GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
      if (html_tag == "section" &&
          GetData()
              .GetString16Attribute(ax::mojom::StringAttribute::kName)
              .empty()) {
        // Do not use ARIA mapping for nameless <section>.
        return ATK_ROLE_SECTION;
      } else {
        // Use ARIA mapping.
        return ATK_ROLE_LANDMARK;
      }
    }
    case ax::mojom::Role::kRootWebArea:
      return ATK_ROLE_DOCUMENT_WEB;
    case ax::mojom::Role::kRow:
      return ATK_ROLE_TABLE_ROW;
    case ax::mojom::Role::kRowHeader:
      return ATK_ROLE_ROW_HEADER;
    case ax::mojom::Role::kRuby:
      return kStaticRole;
    case ax::mojom::Role::kScrollBar:
      return ATK_ROLE_SCROLL_BAR;
    case ax::mojom::Role::kSearch:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSliderThumb:
      return ATK_ROLE_SLIDER;
    case ax::mojom::Role::kSpinButton:
      return ATK_ROLE_SPIN_BUTTON;
    case ax::mojom::Role::kSplitter:
      return ATK_ROLE_SEPARATOR;
    case ax::mojom::Role::kStaticText: {
      switch (static_cast<ax::mojom::TextPosition>(
          GetIntAttribute(ax::mojom::IntAttribute::kTextPosition))) {
        case ax::mojom::TextPosition::kSubscript:
          return kSubscriptRole;
        case ax::mojom::TextPosition::kSuperscript:
          return kSuperscriptRole;
        default:
          break;
      }
      // TODO(jdiggs) This should be ATK_ROLE_STATIC. https://crbug.com/984590
      return ATK_ROLE_TEXT;
    }
    case ax::mojom::Role::kStatus:
      return ATK_ROLE_STATUSBAR;
    case ax::mojom::Role::kSvgRoot:
      return ATK_ROLE_DOCUMENT_FRAME;
    case ax::mojom::Role::kTab:
      return ATK_ROLE_PAGE_TAB;
    case ax::mojom::Role::kTable:
      return ATK_ROLE_TABLE;
    case ax::mojom::Role::kTableHeaderContainer:
      // TODO(accessibility) This mapping is correct, but it doesn't seem to be
      // used. We don't necessarily want to always expose these containers, but
      // we must do so if they are focusable. http://crbug.com/874043
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kTabList:
      return ATK_ROLE_PAGE_TAB_LIST;
    case ax::mojom::Role::kTabPanel:
      return ATK_ROLE_SCROLL_PANE;
    case ax::mojom::Role::kTerm:
      // TODO(accessibility) This mapping should also be applied to the dfn
      // element. http://crbug.com/874411
      return ATK_ROLE_DESCRIPTION_TERM;
    case ax::mojom::Role::kTitleBar:
      return ATK_ROLE_TITLE_BAR;
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kSearchBox:
      if (GetData().HasState(ax::mojom::State::kProtected))
        return ATK_ROLE_PASSWORD_TEXT;
      return ATK_ROLE_ENTRY;
    case ax::mojom::Role::kTextFieldWithComboBox:
      return ATK_ROLE_COMBO_BOX;
    case ax::mojom::Role::kAbbr:
    case ax::mojom::Role::kTime:
      return kStaticRole;
    case ax::mojom::Role::kTimer:
      return ATK_ROLE_TIMER;
    case ax::mojom::Role::kToggleButton:
      return ATK_ROLE_TOGGLE_BUTTON;
    case ax::mojom::Role::kToolbar:
      return ATK_ROLE_TOOL_BAR;
    case ax::mojom::Role::kTooltip:
      return ATK_ROLE_TOOL_TIP;
    case ax::mojom::Role::kTree:
      return ATK_ROLE_TREE;
    case ax::mojom::Role::kTreeItem:
      return ATK_ROLE_TREE_ITEM;
    case ax::mojom::Role::kTreeGrid:
      return ATK_ROLE_TREE_TABLE;
    case ax::mojom::Role::kVideo:
      return ATK_ROLE_VIDEO;
    case ax::mojom::Role::kWebArea:
    case ax::mojom::Role::kWebView:
      return ATK_ROLE_DOCUMENT_WEB;
    case ax::mojom::Role::kWindow:
      // In ATK elements with ATK_ROLE_FRAME are windows with titles and
      // buttons, while those with ATK_ROLE_WINDOW are windows without those
      // elements.
      return ATK_ROLE_FRAME;
    case ax::mojom::Role::kClient:
    case ax::mojom::Role::kDesktop:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kFigcaption:
      return ATK_ROLE_CAPTION;
    case ax::mojom::Role::kKeyboard:
    case ax::mojom::Role::kNone:
    case ax::mojom::Role::kPresentational:
    case ax::mojom::Role::kUnknown:
      return ATK_ROLE_REDUNDANT_OBJECT;
  }
}

void AXPlatformNodeAuraLinux::GetAtkState(AtkStateSet* atk_state_set) {
  AXNodeData data = GetData();

  bool menu_active = !GetActiveMenus().empty();
  if (!menu_active && atk_object_ == g_active_top_level_frame)
    atk_state_set_add_state(atk_state_set, ATK_STATE_ACTIVE);
  if (menu_active &&
      FindAtkObjectParentFrame(GetActiveMenus().back()) == atk_object_)
    atk_state_set_add_state(atk_state_set, ATK_STATE_ACTIVE);

  bool is_minimized = delegate_->IsMinimized();
  if (is_minimized && data.role == ax::mojom::Role::kWindow)
    atk_state_set_add_state(atk_state_set, ATK_STATE_ICONIFIED);

  if (data.HasState(ax::mojom::State::kCollapsed))
    atk_state_set_add_state(atk_state_set, ATK_STATE_EXPANDABLE);
  if (data.HasState(ax::mojom::State::kDefault))
    atk_state_set_add_state(atk_state_set, ATK_STATE_DEFAULT);
  if (data.HasState(ax::mojom::State::kEditable) &&
      data.GetRestriction() != ax::mojom::Restriction::kReadOnly) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_EDITABLE);
  }
  if (data.HasState(ax::mojom::State::kExpanded)) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_EXPANDABLE);
    atk_state_set_add_state(atk_state_set, ATK_STATE_EXPANDED);
  }
  if (data.HasState(ax::mojom::State::kFocusable) ||
      SelectionAndFocusAreTheSame())
    atk_state_set_add_state(atk_state_set, ATK_STATE_FOCUSABLE);
  if (data.HasState(ax::mojom::State::kHorizontal))
    atk_state_set_add_state(atk_state_set, ATK_STATE_HORIZONTAL);
  if (!data.HasState(ax::mojom::State::kInvisible)) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_VISIBLE);
    if (!delegate_->IsOffscreen() && !is_minimized)
      atk_state_set_add_state(atk_state_set, ATK_STATE_SHOWING);
  }
  if (data.HasState(ax::mojom::State::kMultiselectable))
    atk_state_set_add_state(atk_state_set, ATK_STATE_MULTISELECTABLE);
  if (data.HasState(ax::mojom::State::kRequired))
    atk_state_set_add_state(atk_state_set, ATK_STATE_REQUIRED);
  if (data.HasState(ax::mojom::State::kVertical))
    atk_state_set_add_state(atk_state_set, ATK_STATE_VERTICAL);
  if (data.HasState(ax::mojom::State::kVisited))
    atk_state_set_add_state(atk_state_set, ATK_STATE_VISITED);
  if (data.HasIntAttribute(ax::mojom::IntAttribute::kInvalidState) &&
      data.GetIntAttribute(ax::mojom::IntAttribute::kInvalidState) !=
          static_cast<int32_t>(ax::mojom::InvalidState::kFalse))
    atk_state_set_add_state(atk_state_set, ATK_STATE_INVALID_ENTRY);
#if defined(ATK_216)
  if (data.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState) &&
      data.role != ax::mojom::Role::kToggleButton) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_CHECKABLE);
  }
  if (data.HasIntAttribute(ax::mojom::IntAttribute::kHasPopup))
    atk_state_set_add_state(atk_state_set, ATK_STATE_HAS_POPUP);
#endif
  if (data.GetBoolAttribute(ax::mojom::BoolAttribute::kBusy))
    atk_state_set_add_state(atk_state_set, ATK_STATE_BUSY);
  if (data.GetBoolAttribute(ax::mojom::BoolAttribute::kModal))
    atk_state_set_add_state(atk_state_set, ATK_STATE_MODAL);
  if (data.HasBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    atk_state_set_add_state(atk_state_set, ATK_STATE_SELECTABLE);
  if (data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    atk_state_set_add_state(atk_state_set, ATK_STATE_SELECTED);

  if (IsPlainTextField() || IsRichTextField()) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_SELECTABLE_TEXT);
    if (data.HasState(ax::mojom::State::kMultiline))
      atk_state_set_add_state(atk_state_set, ATK_STATE_MULTI_LINE);
    else
      atk_state_set_add_state(atk_state_set, ATK_STATE_SINGLE_LINE);
  }

  if (!GetStringAttribute(ax::mojom::StringAttribute::kAutoComplete).empty() ||
      data.HasState(ax::mojom::State::kAutofillAvailable))
    atk_state_set_add_state(atk_state_set, ATK_STATE_SUPPORTS_AUTOCOMPLETION);

  // Checked state
  const auto checked_state = GetData().GetCheckedState();
  if (checked_state == ax::mojom::CheckedState::kTrue ||
      checked_state == ax::mojom::CheckedState::kMixed) {
    atk_state_set_add_state(atk_state_set, GetAtkStateTypeForCheckableNode());
  }

  switch (GetData().GetRestriction()) {
    case ax::mojom::Restriction::kNone:
      atk_state_set_add_state(atk_state_set, ATK_STATE_ENABLED);
      atk_state_set_add_state(atk_state_set, ATK_STATE_SENSITIVE);
      break;
    case ax::mojom::Restriction::kReadOnly:
#if defined(ATK_216)
      atk_state_set_add_state(atk_state_set, ATK_STATE_READ_ONLY);
#endif
      break;
    default:
      break;
  }

  if (delegate_->GetFocus() == GetOrCreateAtkObject())
    atk_state_set_add_state(atk_state_set, ATK_STATE_FOCUSED);

  // It is insufficient to compare with g_current_activedescendant due to both
  // timing and event ordering for objects which implement AtkSelection and also
  // have an active descendant. For instance, if we check the state set of a
  // selectable child, it will only have ATK_STATE_FOCUSED if we've processed
  // the activedescendant change.
  if (GetActiveDescendantOfCurrentFocused() == GetOrCreateAtkObject())
    atk_state_set_add_state(atk_state_set, ATK_STATE_FOCUSED);
}

struct AtkIntRelation {
  ax::mojom::IntAttribute attribute;
  AtkRelationType relation;
  base::Optional<AtkRelationType> reverse_relation;
};

static AtkIntRelation kIntRelations[] = {
    {ax::mojom::IntAttribute::kMemberOfId, ATK_RELATION_MEMBER_OF,
     base::nullopt},
    {ax::mojom::IntAttribute::kPopupForId, ATK_RELATION_POPUP_FOR,
     base::nullopt},
#if defined(ATK_226)
    {ax::mojom::IntAttribute::kDetailsId, ATK_RELATION_DETAILS,
     ATK_RELATION_DETAILS_FOR},
    {ax::mojom::IntAttribute::kErrormessageId, ATK_RELATION_ERROR_MESSAGE,
     ATK_RELATION_ERROR_FOR},
#endif
};

struct AtkIntListRelation {
  ax::mojom::IntListAttribute attribute;
  AtkRelationType relation;
  base::Optional<AtkRelationType> reverse_relation;
};

static AtkIntListRelation kIntListRelations[] = {
    {ax::mojom::IntListAttribute::kControlsIds, ATK_RELATION_CONTROLLER_FOR,
     ATK_RELATION_CONTROLLED_BY},
    {ax::mojom::IntListAttribute::kDescribedbyIds, ATK_RELATION_DESCRIBED_BY,
     ATK_RELATION_DESCRIPTION_FOR},
    {ax::mojom::IntListAttribute::kFlowtoIds, ATK_RELATION_FLOWS_TO,
     ATK_RELATION_FLOWS_FROM},
    {ax::mojom::IntListAttribute::kLabelledbyIds, ATK_RELATION_LABELLED_BY,
     ATK_RELATION_LABEL_FOR},
};

void AXPlatformNodeAuraLinux::AddRelationToSet(AtkRelationSet* relation_set,
                                               AtkRelationType relation,
                                               AXPlatformNode* target) {
  DCHECK(target);

  // Avoid adding self-referential relations.
  if (target == this)
    return;

  // If we were compiled with a newer version of ATK than the runtime version,
  // it's possible that we might try to add a relation that doesn't exist in
  // the runtime version of the AtkRelationType enum. This will cause a runtime
  // error, so return early here if we are about to do that.
  static base::Optional<int> max_relation_type = base::nullopt;
  if (!max_relation_type.has_value()) {
    GEnumClass* enum_class =
        G_ENUM_CLASS(g_type_class_ref(atk_relation_type_get_type()));
    max_relation_type = enum_class->maximum;
    g_type_class_unref(enum_class);
  }
  if (relation > max_relation_type.value())
    return;

  atk_relation_set_add_relation_by_type(relation_set, relation,
                                        target->GetNativeViewAccessible());
}

AtkRelationSet* AXPlatformNodeAuraLinux::GetAtkRelations() {
  AtkRelationSet* relation_set = atk_relation_set_new();

  if (embedded_document_) {
    atk_relation_set_add_relation_by_type(relation_set, ATK_RELATION_EMBEDS,
                                          embedded_document_);
  }

  if (embedding_window_) {
    atk_relation_set_add_relation_by_type(
        relation_set, ATK_RELATION_EMBEDDED_BY, embedding_window_);
  }

  // For each possible relation defined by an IntAttribute, we test that
  // attribute and then look for reverse relations. AddRelationToSet handles
  // discarding self-referential relations.
  for (unsigned i = 0; i < G_N_ELEMENTS(kIntRelations); i++) {
    const AtkIntRelation& relation = kIntRelations[i];

    if (AXPlatformNode* target =
            GetDelegate()->GetTargetNodeForRelation(relation.attribute))
      AddRelationToSet(relation_set, relation.relation, target);

    if (!relation.reverse_relation.has_value())
      continue;

    std::set<AXPlatformNode*> target_ids =
        GetDelegate()->GetReverseRelations(relation.attribute);
    for (AXPlatformNode* target : target_ids) {
      AddRelationToSet(relation_set, relation.reverse_relation.value(), target);
    }
  }

  // Now we do the same for each possible relation defined by an
  // IntListAttribute. In this case we need to handle each target in the list.
  for (unsigned i = 0; i < G_N_ELEMENTS(kIntListRelations); i++) {
    const AtkIntListRelation& relation = kIntListRelations[i];

    std::set<AXPlatformNode*> targets =
        GetDelegate()->GetTargetNodesForRelation(relation.attribute);
    for (AXPlatformNode* target : targets)
      AddRelationToSet(relation_set, relation.relation, target);

    if (!relation.reverse_relation.has_value())
      continue;

    std::set<AXPlatformNode*> reverse_target_ids =
        GetDelegate()->GetReverseRelations(relation.attribute);
    for (AXPlatformNode* target : reverse_target_ids) {
      AddRelationToSet(relation_set, relation.reverse_relation.value(), target);
    }
  }

  return relation_set;
}

AXPlatformNodeAuraLinux::AXPlatformNodeAuraLinux() = default;

AXPlatformNodeAuraLinux::~AXPlatformNodeAuraLinux() {
  if (g_current_selected == this)
    g_current_selected = nullptr;

  DestroyAtkObjects();

  SetWeakGPtrToAtkObject(&embedded_document_, nullptr);
  SetWeakGPtrToAtkObject(&embedding_window_, nullptr);
}

void AXPlatformNodeAuraLinux::Destroy() {
  DestroyAtkObjects();
  AXPlatformNodeBase::Destroy();
}

void AXPlatformNodeAuraLinux::Init(AXPlatformNodeDelegate* delegate) {
  // Initialize ATK.
  AXPlatformNodeBase::Init(delegate);

  // Only create the AtkObject if we know enough information.
  if (GetData().role != ax::mojom::Role::kUnknown)
    GetOrCreateAtkObject();
}

void AXPlatformNodeAuraLinux::EnsureAtkObjectIsValid() {
  if (atk_object_) {
    // If the object's role changes and that causes its
    // interface mask to change, we need to create a new
    // AtkObject for it.
    int interface_mask = GetGTypeInterfaceMask();
    if (interface_mask != interface_mask_)
      DestroyAtkObjects();
  }

  if (!atk_object_) {
    GetOrCreateAtkObject();
  }
}

gfx::NativeViewAccessible AXPlatformNodeAuraLinux::GetNativeViewAccessible() {
  return GetOrCreateAtkObject();
}

gfx::NativeViewAccessible AXPlatformNodeAuraLinux::GetOrCreateAtkObject() {
  if (!atk_object_) {
    atk_object_ = CreateAtkObject();
  }
  return atk_object_;
}

void AXPlatformNodeAuraLinux::OnActiveDescendantChanged() {
  // Active-descendant-changed notifications are typically only relevant when
  // the change is within the focused widget.
  if (GetOrCreateAtkObject() != g_current_focused)
    return;

  AtkObject* descendant = GetActiveDescendantOfCurrentFocused();
  if (descendant == g_current_active_descendant)
    return;

  // If selection and focus are the same, when the active descendant changes
  // as a result of selection, a focus event will be emitted. We don't want to
  // emit duplicate notifications.
  {
    auto* node = AtkObjectToAXPlatformNodeAuraLinux(descendant);
    if (node && node->SelectionAndFocusAreTheSame())
      return;
  }

  // While there is an ATK active-descendant-changed event, it is meant for
  // objects which manage their descendants (and claim to do so). The Core-AAM
  // specification states that focus events should be emitted when the active
  // descendant changes. This behavior is also consistent with Gecko.
  if (g_current_active_descendant) {
    g_signal_emit_by_name(g_current_active_descendant, "focus-event", false);
    atk_object_notify_state_change(ATK_OBJECT(g_current_active_descendant),
                                   ATK_STATE_FOCUSED, false);
  }

  SetWeakGPtrToAtkObject(&g_current_active_descendant, descendant);
  if (g_current_active_descendant) {
    g_signal_emit_by_name(g_current_active_descendant, "focus-event", true);
    atk_object_notify_state_change(ATK_OBJECT(g_current_active_descendant),
                                   ATK_STATE_FOCUSED, true);
  }
}

void AXPlatformNodeAuraLinux::OnCheckedStateChanged() {
  atk_object_notify_state_change(
      ATK_OBJECT(GetOrCreateAtkObject()), GetAtkStateTypeForCheckableNode(),
      GetData().GetCheckedState() != ax::mojom::CheckedState::kFalse);
}

void AXPlatformNodeAuraLinux::OnExpandedStateChanged(bool is_expanded) {
  // When a list box is expanded, it becomes visible. This means that it might
  // now have a different role (the role for hidden Views is kUnknown).  We
  // need to recreate the AtkObject in this case because a change in roles
  // might imply a change in ATK interfaces implemented.
  EnsureAtkObjectIsValid();

  atk_object_notify_state_change(ATK_OBJECT(GetOrCreateAtkObject()),
                                 ATK_STATE_EXPANDED, is_expanded);
}

void AXPlatformNodeAuraLinux::OnMenuPopupStart() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  AtkObject* parent_frame = FindAtkObjectParentFrame(atk_object);
  if (!parent_frame)
    return;

  // Exit early if kMenuPopupStart is sent multiple times for the same menu.
  std::vector<AtkObject*>& active_menus = GetActiveMenus();
  bool menu_already_open = !active_menus.empty();
  if (menu_already_open && active_menus.back() == atk_object)
    return;

  // We also want to inform the AT that menu the is now showing. Normally this
  // event is not fired because the menu will be created with the
  // ATK_STATE_SHOWING already set to TRUE.
  atk_object_notify_state_change(atk_object, ATK_STATE_SHOWING, TRUE);

  // We need to compute this before modifying the active menu stack.
  AtkObject* previous_active_frame = ComputeActiveTopLevelFrame();

  active_menus.push_back(atk_object);

  // We exit early if the newly activated menu has the same AtkWindow as the
  // previous one.
  if (previous_active_frame == parent_frame)
    return;
  if (previous_active_frame) {
    g_signal_emit_by_name(previous_active_frame, "deactivate");
    atk_object_notify_state_change(previous_active_frame, ATK_STATE_ACTIVE,
                                   FALSE);
  }
  g_signal_emit_by_name(parent_frame, "activate");
  atk_object_notify_state_change(parent_frame, ATK_STATE_ACTIVE, TRUE);
}

void AXPlatformNodeAuraLinux::OnMenuPopupHide() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  AtkObject* parent_frame = FindAtkObjectParentFrame(atk_object);
  if (!parent_frame)
    return;

  atk_object_notify_state_change(atk_object, ATK_STATE_SHOWING, FALSE);

  // kMenuPopupHide may be called multiple times for the same menu, so only
  // remove it if our parent frame matches the most recently opened menu.
  std::vector<AtkObject*>& active_menus = GetActiveMenus();
  if (active_menus.empty())
    return;

  // When multiple levels of menu are closed at once, they may be hidden out
  // of order. When this happens, we just remove the open menu from the stack.
  if (active_menus.back() != atk_object) {
    auto it = std::find(active_menus.rbegin(), active_menus.rend(), atk_object);
    if (it != active_menus.rend()) {
      // We used a reverse iterator, so we need to convert it into a normal
      // iterator to use it for std::vector::erase(...).
      auto to_remove = --(it.base());
      active_menus.erase(to_remove);
    }
    return;
  }

  active_menus.pop_back();

  // We exit early if the newly activated menu has the same AtkWindow as the
  // previous one.
  AtkObject* new_active_item = ComputeActiveTopLevelFrame();
  if (new_active_item == parent_frame)
    return;
  g_signal_emit_by_name(parent_frame, "deactivate");
  atk_object_notify_state_change(parent_frame, ATK_STATE_ACTIVE, FALSE);
  if (new_active_item) {
    g_signal_emit_by_name(new_active_item, "activate");
    atk_object_notify_state_change(new_active_item, ATK_STATE_ACTIVE, TRUE);
  }
}

void AXPlatformNodeAuraLinux::OnMenuPopupEnd() {
  if (!GetActiveMenus().empty() && g_active_top_level_frame &&
      ComputeActiveTopLevelFrame() != g_active_top_level_frame) {
    g_signal_emit_by_name(g_active_top_level_frame, "activate");
    atk_object_notify_state_change(g_active_top_level_frame, ATK_STATE_ACTIVE,
                                   TRUE);
  }

  GetActiveMenus().clear();
}

void AXPlatformNodeAuraLinux::OnWindowActivated() {
  AtkObject* parent_frame = FindAtkObjectParentFrame(GetOrCreateAtkObject());
  if (!parent_frame || parent_frame == g_active_top_level_frame)
    return;

  SetActiveTopLevelFrame(parent_frame);

  g_signal_emit_by_name(parent_frame, "activate");
  atk_object_notify_state_change(parent_frame, ATK_STATE_ACTIVE, TRUE);

  // We also send a focus event for the currently focused element, so that
  // the user knows where the focus is when the toplevel window regains focus.
  if (g_current_focused &&
      IsFrameAncestorOfAtkObject(parent_frame, g_current_focused)) {
    g_signal_emit_by_name(g_current_focused, "focus-event", true);
    atk_object_notify_state_change(ATK_OBJECT(g_current_focused),
                                   ATK_STATE_FOCUSED, true);
  }
}

void AXPlatformNodeAuraLinux::OnWindowDeactivated() {
  AtkObject* parent_frame = FindAtkObjectParentFrame(GetOrCreateAtkObject());
  if (!parent_frame || parent_frame != g_active_top_level_frame)
    return;

  SetActiveTopLevelFrame(nullptr);

  g_signal_emit_by_name(parent_frame, "deactivate");
  atk_object_notify_state_change(parent_frame, ATK_STATE_ACTIVE, FALSE);
}

void AXPlatformNodeAuraLinux::OnWindowVisibilityChanged() {
  AtkObject* atk_object = GetOrCreateAtkObject();

  if (atk_object_get_role(atk_object) != ATK_ROLE_FRAME)
    return;

  bool minimized = delegate_->IsMinimized();
  if (minimized == was_minimized_)
    return;

  was_minimized_ = minimized;
  if (minimized)
    g_signal_emit_by_name(atk_object, "minimize");
  else
    g_signal_emit_by_name(atk_object, "restore");
  atk_object_notify_state_change(atk_object, ATK_STATE_ICONIFIED, minimized);
}

void AXPlatformNodeAuraLinux::OnFocused() {
  AtkObject* atk_object = GetOrCreateAtkObject();

  if (atk_object_get_role(atk_object) == ATK_ROLE_FRAME) {
    OnWindowActivated();
    return;
  }

  if (atk_object == g_current_focused)
    return;

  if (g_current_focused) {
    g_signal_emit_by_name(g_current_focused, "focus-event", false);
    atk_object_notify_state_change(ATK_OBJECT(g_current_focused),
                                   ATK_STATE_FOCUSED, false);
  }

  SetWeakGPtrToAtkObject(&g_current_focused, atk_object);
  g_signal_emit_by_name(atk_object, "focus-event", true);
  atk_object_notify_state_change(ATK_OBJECT(atk_object), ATK_STATE_FOCUSED,
                                 true);
}

void AXPlatformNodeAuraLinux::OnSelected() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (g_current_selected && !g_current_selected->GetData().GetBoolAttribute(
                                ax::mojom::BoolAttribute::kSelected)) {
    atk_object_notify_state_change(
        ATK_OBJECT(g_current_selected->GetOrCreateAtkObject()),
        ATK_STATE_SELECTED, false);
  }

  g_current_selected = this;
  if (ATK_IS_OBJECT(atk_object)) {
    atk_object_notify_state_change(ATK_OBJECT(atk_object), ATK_STATE_SELECTED,
                                   true);
  }

  if (SelectionAndFocusAreTheSame())
    OnFocused();
}

void AXPlatformNodeAuraLinux::OnSelectedChildrenChanged() {
  g_signal_emit_by_name(GetOrCreateAtkObject(), "selection-changed", true);
}

bool AXPlatformNodeAuraLinux::SelectionAndFocusAreTheSame() {
  if (AXPlatformNodeBase* container = GetSelectionContainer()) {
    ax::mojom::Role role = container->GetData().role;

    // In the browser UI, menus and their descendants emit selection-related
    // events only, but we also want to emit platform focus-related events,
    // so we treat selection and focus the same for browser UI.
    if (role == ax::mojom::Role::kMenuBar || role == ax::mojom::Role::kMenu)
      return !GetDelegate()->IsWebContent();
    if (role == ax::mojom::Role::kListBox &&
        !container->GetData().HasState(ax::mojom::State::kMultiselectable)) {
      return container->GetDelegate()->GetFocus() ==
             container->GetNativeViewAccessible();
    }
  }

  // TODO(accessibility): GetSelectionContainer returns nullptr when the current
  // object is a descendant of a select element with a size of 1. Intentional?
  // For now, handle that scenario here.
  //
  // If the selection is changing on a collapsed select element, focus remains
  // on the select element and not the newly-selected descendant.
  if (AXPlatformNodeBase* parent =
          AtkObjectToAXPlatformNodeAuraLinux(GetParent())) {
    if (parent->GetData().role == ax::mojom::Role::kMenuListPopup)
      return !parent->GetData().HasState(ax::mojom::State::kInvisible);
  }

  return false;
}

void AXPlatformNodeAuraLinux::OnTextSelectionChanged() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!EmitsAtkTextEvents(atk_object)) {
    if (auto* parent = AtkObjectToAXPlatformNodeAuraLinux(GetParent()))
      parent->OnTextSelectionChanged();
    return;
  }

  DCHECK(ATK_IS_TEXT(atk_object));

  std::pair<int, int> new_selection;
  GetSelectionOffsets(&new_selection.first, &new_selection.second);
  std::pair<int, int> old_selection = text_selection_;
  text_selection_ = new_selection;

  // ATK does not consider a collapsed selection a selection, so
  // when the collapsed selection changes (caret movement), we should
  // avoid sending text-selection-changed events.
  bool has_selection = SelectionOffsetsIndicateSelection(new_selection);
  bool had_selection = SelectionOffsetsIndicateSelection(old_selection);
  if (has_selection != had_selection ||
      (has_selection && new_selection != old_selection)) {
    g_signal_emit_by_name(atk_object, "text-selection-changed");
  }

  if (HasCaret() && new_selection.second != old_selection.second) {
    g_signal_emit_by_name(atk_object, "text-caret-moved",
                          UTF16ToUnicodeOffsetInText(new_selection.second));
  }
}

bool AXPlatformNodeAuraLinux::SupportsSelectionWithAtkSelection() {
  return SupportsToggle(GetData().role) ||
         GetData().role == ax::mojom::Role::kListBoxOption;
}

void AXPlatformNodeAuraLinux::OnDescriptionChanged() {
  std::string description;
  GetStringAttribute(ax::mojom::StringAttribute::kDescription, &description);

  AtkPropertyValues property_values;
  property_values.property_name = "accessible-description";
  property_values.new_value = G_VALUE_INIT;
  g_value_init(&property_values.new_value, G_TYPE_STRING);
  g_value_set_string(&property_values.new_value, description.c_str());
  g_signal_emit_by_name(G_OBJECT(GetOrCreateAtkObject()),
                        "property-change::accessible-description",
                        &property_values, nullptr);
  g_value_unset(&property_values.new_value);
}

void AXPlatformNodeAuraLinux::OnValueChanged() {
  if (!IsRangeValueSupported(GetData()))
    return;

  float float_val;
  if (!GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange, &float_val))
    return;

  AtkPropertyValues property_values;
  property_values.property_name = "accessible-value";

  property_values.new_value = G_VALUE_INIT;
  g_value_init(&property_values.new_value, G_TYPE_DOUBLE);
  g_value_set_double(&property_values.new_value,
                     static_cast<double>(float_val));
  g_signal_emit_by_name(G_OBJECT(GetOrCreateAtkObject()),
                        "property-change::accessible-value", &property_values,
                        nullptr);
}

void AXPlatformNodeAuraLinux::OnNameChanged() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  std::string previous_accessible_name = accessible_name_;
  // Calling atk_object_get_name will update the value of accessible_name_.
  if (!g_strcmp0(atk_object_get_name(atk_object),
                 previous_accessible_name.c_str()))
    return;

  g_object_notify(G_OBJECT(atk_object), "accessible-name");
}

void AXPlatformNodeAuraLinux::OnDocumentTitleChanged() {
  if (!g_active_top_level_frame)
    return;

  // We always want to notify on the top frame.
  AXPlatformNodeAuraLinux* window =
      AtkObjectToAXPlatformNodeAuraLinux(g_active_top_level_frame);
  if (window)
    window->OnNameChanged();
}

void AXPlatformNodeAuraLinux::OnSubtreeCreated() {
  // We might not have a parent, in that case we don't need to send the event.
  // We also don't want to notify if this is an ignored node
  if (!GetParent() || GetData().HasState(ax::mojom::State::kIgnored))
    return;
  g_signal_emit_by_name(GetParent(), "children-changed::add",
                        GetIndexInParent(), GetOrCreateAtkObject());
}

void AXPlatformNodeAuraLinux::OnSubtreeWillBeDeleted() {
  // There is a chance there won't be a parent as we're in the deletion process.
  // We also don't want to notify if this is an ignored node
  if (!GetParent() || GetData().HasState(ax::mojom::State::kIgnored))
    return;

  g_signal_emit_by_name(GetParent(), "children-changed::remove",
                        GetIndexInParent(), GetOrCreateAtkObject());
}

void AXPlatformNodeAuraLinux::OnInvalidStatusChanged() {
  atk_object_notify_state_change(
      ATK_OBJECT(GetOrCreateAtkObject()), ATK_STATE_INVALID_ENTRY,
      GetData().GetInvalidState() != ax::mojom::InvalidState::kFalse);
}

void AXPlatformNodeAuraLinux::NotifyAccessibilityEvent(
    ax::mojom::Event event_type) {
  switch (event_type) {
    // There are three types of messages that we receive for popup menus. Each
    // time a popup menu is shown, we get a kMenuPopupStart message. This
    // includes if the menu is hidden and then re-shown. When a menu is hidden
    // we receive the kMenuPopupHide message. Finally, when the entire menu is
    // closed we receive the kMenuPopupEnd message for the parent menu and all
    // of the submenus that were opened when navigating through the menu.
    case ax::mojom::Event::kMenuPopupEnd:
      OnMenuPopupEnd();
      break;
    case ax::mojom::Event::kMenuPopupHide:
      OnMenuPopupHide();
      break;
    case ax::mojom::Event::kMenuPopupStart:
      OnMenuPopupStart();
      break;
    case ax::mojom::Event::kActiveDescendantChanged:
      OnActiveDescendantChanged();
      break;
    case ax::mojom::Event::kCheckedStateChanged:
      OnCheckedStateChanged();
      break;
    case ax::mojom::Event::kExpandedChanged:
      OnExpandedStateChanged(GetData().HasState(ax::mojom::State::kExpanded));
      break;
    case ax::mojom::Event::kFocus:
    case ax::mojom::Event::kFocusContext:
      OnFocused();
      break;
    case ax::mojom::Event::kSelection:
      OnSelected();
      // When changing tabs also fire a name changed event.
      if (GetData().role == ax::mojom::Role::kTab)
        OnDocumentTitleChanged();
      break;
    case ax::mojom::Event::kSelectedChildrenChanged:
      OnSelectedChildrenChanged();
      break;
    case ax::mojom::Event::kTextChanged:
      OnNameChanged();
      break;
    case ax::mojom::Event::kTextSelectionChanged:
      OnTextSelectionChanged();
      break;
    case ax::mojom::Event::kValueChanged:
      OnValueChanged();
      break;
    case ax::mojom::Event::kInvalidStatusChanged:
      OnInvalidStatusChanged();
      break;
    case ax::mojom::Event::kWindowActivated:
      OnWindowActivated();
      break;
    case ax::mojom::Event::kWindowDeactivated:
      OnWindowDeactivated();
      break;
    case ax::mojom::Event::kWindowVisibilityChanged:
      OnWindowVisibilityChanged();
      break;
    case ax::mojom::Event::kLoadComplete:
    case ax::mojom::Event::kDocumentTitleChanged:
      // Sometimes, e.g. upon navigating away from the page, the tree is
      // rebuilt rather than modified. The kDocumentTitleChanged event occurs
      // prior to the rebuild and so is added on the previous root node. When
      // the tree is rebuilt and the old node removed, the events on the old
      // node are removed and no new kDocumentTitleChanged will be emitted. To
      // ensure we still fire the event, though, we also pay attention to
      // kLoadComplete.
      OnDocumentTitleChanged();
      break;
    default:
      break;
  }
}

base::Optional<std::pair<int, int>>
AXPlatformNodeAuraLinux::GetEmbeddedObjectIndicesForId(int id) {
  auto iterator =
      std::find(hypertext_.hyperlinks.begin(), hypertext_.hyperlinks.end(), id);
  if (iterator == hypertext_.hyperlinks.end())
    return base::nullopt;
  int hyperlink_index = std::distance(hypertext_.hyperlinks.begin(), iterator);

  auto offset = std::find_if(hypertext_.hyperlink_offset_to_index.begin(),
                             hypertext_.hyperlink_offset_to_index.end(),
                             [&](const std::pair<int32_t, int32_t>& pair) {
                               return pair.second == hyperlink_index;
                             });
  if (offset == hypertext_.hyperlink_offset_to_index.end())
    return base::nullopt;

  return std::make_pair(UTF16ToUnicodeOffsetInText(offset->first),
                        UTF16ToUnicodeOffsetInText(offset->first + 1));
}

void AXPlatformNodeAuraLinux::UpdateHypertext() {
  EnsureAtkObjectIsValid();
  AXHypertext old_hypertext = hypertext_;
  base::OffsetAdjuster::Adjustments old_adjustments = GetHypertextAdjustments();

  UpdateComputedHypertext();
  text_unicode_adjustments_ = base::nullopt;

  if ((!GetData().HasState(ax::mojom::State::kEditable) ||
       GetData().GetRestriction() == ax::mojom::Restriction::kReadOnly) &&
      !IsInLiveRegion()) {
    return;
  }

  size_t shared_prefix, old_len, new_len;
  ComputeHypertextRemovedAndInserted(old_hypertext, &shared_prefix, &old_len,
                                     &new_len);

  AtkObject* atk_object = GetOrCreateAtkObject();
  DCHECK(ATK_IS_TEXT(atk_object));

  if (!EmitsAtkTextEvents(atk_object))
    return;

  if (old_len > 0) {
    base::string16 removed_substring =
        old_hypertext.hypertext.substr(shared_prefix, old_len);

    size_t shared_unicode_prefix = shared_prefix;
    base::OffsetAdjuster::AdjustOffset(old_adjustments, &shared_unicode_prefix);
    size_t shared_unicode_suffix = shared_prefix + old_len;
    base::OffsetAdjuster::AdjustOffset(old_adjustments, &shared_unicode_suffix);

    g_signal_emit_by_name(
        atk_object, "text-remove",
        shared_unicode_prefix,                  // position of removal
        shared_unicode_suffix - shared_prefix,  // length of removal
        base::UTF16ToUTF8(removed_substring).c_str());
  }

  if (new_len > 0) {
    base::string16 inserted_substring =
        hypertext_.hypertext.substr(shared_prefix, new_len);
    size_t shared_unicode_prefix = UTF16ToUnicodeOffsetInText(shared_prefix);
    size_t shared_unicode_suffix =
        UTF16ToUnicodeOffsetInText(shared_prefix + new_len);
    g_signal_emit_by_name(
        atk_object, "text-insert",
        shared_unicode_prefix,                          // position of insertion
        shared_unicode_suffix - shared_unicode_prefix,  // length of insertion
        base::UTF16ToUTF8(inserted_substring).c_str());
  }
}

const AXHypertext& AXPlatformNodeAuraLinux::GetAXHypertext() {
  return hypertext_;
}

const base::OffsetAdjuster::Adjustments&
AXPlatformNodeAuraLinux::GetHypertextAdjustments() {
  if (text_unicode_adjustments_.has_value())
    return *text_unicode_adjustments_;

  text_unicode_adjustments_.emplace();

  base::string16 text = GetHypertext();
  int32_t text_length = text.size();
  for (int32_t i = 0; i < text_length; i++) {
    uint32_t code_point;
    size_t original_i = i;
    base::ReadUnicodeCharacter(text.c_str(), text_length + 1, &i, &code_point);

    if ((i - original_i + 1) != 1) {
      text_unicode_adjustments_->push_back(
          base::OffsetAdjuster::Adjustment(original_i, i - original_i + 1, 1));
    }
  }

  return *text_unicode_adjustments_;
}

size_t AXPlatformNodeAuraLinux::UTF16ToUnicodeOffsetInText(
    size_t utf16_offset) {
  size_t unicode_offset = utf16_offset;
  base::OffsetAdjuster::AdjustOffset(GetHypertextAdjustments(),
                                     &unicode_offset);
  return unicode_offset;
}

size_t AXPlatformNodeAuraLinux::UnicodeToUTF16OffsetInText(int unicode_offset) {
  if (unicode_offset == kStringLengthOffset)
    return GetHypertext().size();

  size_t utf16_offset = unicode_offset;
  base::OffsetAdjuster::UnadjustOffset(GetHypertextAdjustments(),
                                       &utf16_offset);
  return utf16_offset;
}

int AXPlatformNodeAuraLinux::GetIndexInParent() {
  if (!GetParent())
    return -1;

  return delegate_->GetIndexInParent();
}

gfx::Vector2d AXPlatformNodeAuraLinux::GetParentOriginInScreenCoordinates()
    const {
  AtkObject* parent = GetParent();
  if (!parent)
    return gfx::Vector2d();

  const AXPlatformNode* parent_node =
      AXPlatformNode::FromNativeViewAccessible(parent);
  if (!parent)
    return gfx::Vector2d();

  return parent_node->GetDelegate()
      ->GetBoundsRect(AXCoordinateSystem::kScreen,
                      AXClippingBehavior::kUnclipped)
      .OffsetFromOrigin();
}

gfx::Vector2d AXPlatformNodeAuraLinux::GetParentFrameOriginInScreenCoordinates()
    const {
  AtkObject* frame = FindAtkObjectParentFrame(atk_object_);
  if (!frame)
    return gfx::Vector2d();

  const AXPlatformNode* frame_node =
      AXPlatformNode::FromNativeViewAccessible(frame);
  if (!frame_node)
    return gfx::Vector2d();

  return frame_node->GetDelegate()
      ->GetBoundsRect(AXCoordinateSystem::kScreen,
                      AXClippingBehavior::kUnclipped)
      .OffsetFromOrigin();
}

gfx::Rect AXPlatformNodeAuraLinux::GetExtentsRelativeToAtkCoordinateType(
    AtkCoordType coord_type) const {
  gfx::Rect extents = delegate_->GetBoundsRect(AXCoordinateSystem::kScreen,
                                               AXClippingBehavior::kUnclipped);
  if (coord_type == ATK_XY_WINDOW) {
    gfx::Vector2d frame_origin = -GetParentOriginInScreenCoordinates();
    extents.Offset(frame_origin);
  }

  return extents;
}

void AXPlatformNodeAuraLinux::GetExtents(gint* x,
                                         gint* y,
                                         gint* width,
                                         gint* height,
                                         AtkCoordType coord_type) {
  gfx::Rect extents = GetExtentsRelativeToAtkCoordinateType(coord_type);
  if (x)
    *x = extents.x();
  if (y)
    *y = extents.y();
  if (width)
    *width = extents.width();
  if (height)
    *height = extents.height();
}

void AXPlatformNodeAuraLinux::GetPosition(gint* x, gint* y,
                                          AtkCoordType coord_type) {
  gfx::Rect extents = GetExtentsRelativeToAtkCoordinateType(coord_type);
  if (x)
    *x = extents.x();
  if (y)
    *y = extents.y();
}

void AXPlatformNodeAuraLinux::GetSize(gint* width, gint* height) {
  gfx::Rect rect_size = gfx::ToEnclosingRect(GetData().relative_bounds.bounds);
  if (width)
    *width = rect_size.width();
  if (height)
    *height = rect_size.height();
}

gfx::NativeViewAccessible
AXPlatformNodeAuraLinux::HitTestSync(gint x, gint y, AtkCoordType coord_type) {
  if (coord_type == ATK_XY_WINDOW) {
    if (AtkObject* atk_object = GetParent()) {
      gfx::Point window_coords = FindAtkObjectParentCoords(atk_object);
      x += window_coords.x();
      y += window_coords.y();
    }
  }

  return delegate_->HitTestSync(x, y);
}

bool AXPlatformNodeAuraLinux::GrabFocus() {
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  return delegate_->AccessibilityPerformAction(action_data);
}

bool AXPlatformNodeAuraLinux::
    GrabFocusOrSetSequentialFocusNavigationStartingPointAtOffset(int offset) {
  int child_count = delegate_->GetChildCount();
  if (IsPlainTextField() || child_count == 0)
    return GrabFocusOrSetSequentialFocusNavigationStartingPoint();

  // When this node has children, we walk through them to figure out what child
  // node should get focus. We are essentially repeating the process used when
  // building the hypertext here.
  int current_offset = 0;
  for (int i = 0; i < child_count; ++i) {
    auto* child =
        AtkObjectToAXPlatformNodeAuraLinux(delegate_->ChildAtIndex(i));
    if (!child)
      continue;

    if (child->IsTextOnlyObject()) {
      current_offset +=
          child->GetString16Attribute(ax::mojom::StringAttribute::kName).size();
    } else {
      // Add an offset for the embedded character.
      current_offset += 1;
    }

    // If the offset is larger than our size, try to work with the last child,
    // which is also the behavior of SetCaretOffset.
    if (offset <= current_offset || i == child_count - 1) {
      // When deciding to do this on the parent or the child we want to err
      // toward doing it on a focusable node. If neither node is focusable, we
      // should call GrabFocusOrSetSequentialFocusNavigationStartingPoint on
      // the child.
      bool can_focus_node = GetData().HasState(ax::mojom::State::kFocusable);
      bool can_focus_child =
          child->GetData().HasState(ax::mojom::State::kFocusable);
      if (can_focus_node && !can_focus_child)
        return GrabFocusOrSetSequentialFocusNavigationStartingPoint();
      else
        return child->GrabFocusOrSetSequentialFocusNavigationStartingPoint();
    }
  }

  NOTREACHED();
  return false;
}

bool AXPlatformNodeAuraLinux::
    GrabFocusOrSetSequentialFocusNavigationStartingPoint() {
  if (GetData().HasState(ax::mojom::State::kFocusable) ||
      SelectionAndFocusAreTheSame()) {
    return GrabFocus();
  }

  AXActionData action_data;
  action_data.action =
      ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint;
  return delegate_->AccessibilityPerformAction(action_data);
}

bool AXPlatformNodeAuraLinux::DoDefaultAction() {
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  return delegate_->AccessibilityPerformAction(action_data);
}

const gchar* AXPlatformNodeAuraLinux::GetDefaultActionName() {
  int action;
  if (!GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb, &action))
    return nullptr;

  base::string16 action_verb = ActionVerbToUnlocalizedString(
      static_cast<ax::mojom::DefaultActionVerb>(action));

  ATK_AURALINUX_RETURN_STRING(base::UTF16ToUTF8(action_verb));
}

AtkAttributeSet* AXPlatformNodeAuraLinux::GetAtkAttributes() {
  AtkAttributeSet* attribute_list = nullptr;
  ComputeAttributes(&attribute_list);
  return attribute_list;
}

AtkStateType AXPlatformNodeAuraLinux::GetAtkStateTypeForCheckableNode() {
  if (GetData().GetCheckedState() == ax::mojom::CheckedState::kMixed)
    return ATK_STATE_INDETERMINATE;
  if (GetData().role == ax::mojom::Role::kToggleButton)
    return ATK_STATE_PRESSED;
  return ATK_STATE_CHECKED;
}

// AtkDocumentHelpers

const gchar* AXPlatformNodeAuraLinux::GetDocumentAttributeValue(
    const gchar* attribute) const {
  if (!g_ascii_strcasecmp(attribute, "DocType"))
    return delegate_->GetTreeData().doctype.c_str();
  else if (!g_ascii_strcasecmp(attribute, "MimeType"))
    return delegate_->GetTreeData().mimetype.c_str();
  else if (!g_ascii_strcasecmp(attribute, "Title"))
    return delegate_->GetTreeData().title.c_str();
  else if (!g_ascii_strcasecmp(attribute, "URI"))
    return delegate_->GetTreeData().url.c_str();

  return nullptr;
}

AtkAttributeSet* AXPlatformNodeAuraLinux::GetDocumentAttributes() const {
  AtkAttributeSet* attribute_set = nullptr;
  const gchar* doc_attributes[] = {"DocType", "MimeType", "Title", "URI"};
  const gchar* value = nullptr;

  for (unsigned i = 0; i < G_N_ELEMENTS(doc_attributes); i++) {
    value = GetDocumentAttributeValue(doc_attributes[i]);
    if (value) {
      attribute_set = PrependAtkAttributeToAtkAttributeSet(
          doc_attributes[i], value, attribute_set);
    }
  }

  return attribute_set;
}

//
// AtkHyperlink helpers
//

AtkHyperlink* AXPlatformNodeAuraLinux::GetAtkHyperlink() {
  if (atk_hyperlink_)
    return atk_hyperlink_;

  atk_hyperlink_ =
      ATK_HYPERLINK(g_object_new(AX_PLATFORM_ATK_HYPERLINK_TYPE, 0));
  ax_platform_atk_hyperlink_set_object(
      AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink_), this);

  auto* parent = AtkObjectToAXPlatformNodeAuraLinux(GetParent());
  if (!parent)
    return atk_hyperlink_;

  base::Optional<std::pair<int, int>> indices =
      parent->GetEmbeddedObjectIndicesForId(GetUniqueId());
  if (!indices.has_value())
    return atk_hyperlink_;

  ax_platform_atk_hyperlink_set_indices(
      AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink_), indices->first,
      indices->second);

  return atk_hyperlink_;
}

//
// Misc helpers
//

void AXPlatformNodeAuraLinux::GetFloatAttributeInGValue(
    ax::mojom::FloatAttribute attr,
    GValue* value) {
  float float_val;
  if (GetFloatAttribute(attr, &float_val)) {
    memset(value, 0, sizeof(*value));
    g_value_init(value, G_TYPE_FLOAT);
    g_value_set_float(value, float_val);
  }
}

void AXPlatformNodeAuraLinux::AddAttributeToList(const char* name,
                                                 const char* value,
                                                 AtkAttributeSet** attributes) {
  *attributes = PrependAtkAttributeToAtkAttributeSet(name, value, *attributes);
}

void AXPlatformNodeAuraLinux::SetEmbeddedDocument(
    AtkObject* new_embedded_document) {
  SetWeakGPtrToAtkObject(&embedded_document_, new_embedded_document);
}

void AXPlatformNodeAuraLinux::SetEmbeddingWindow(
    AtkObject* new_embedding_window) {
  SetWeakGPtrToAtkObject(&embedding_window_, new_embedding_window);
}

base::string16 AXPlatformNodeAuraLinux::GetHypertext() const {
  // Special case allows us to get text even in non-HTML case, e.g. browser UI.
  if (IsPlainTextField())
    return GetString16Attribute(ax::mojom::StringAttribute::kValue);

  // Hypertext of platform leaves, which internally are composite objects, are
  // represented with the inner text of the composite object.
  if (IsChildOfLeaf())
    return GetInnerText();

  return hypertext_.hypertext;
}

int AXPlatformNodeAuraLinux::GetCaretOffset() {
  if (!HasCaret())
    return -1;

  int selection_start, selection_end;
  GetSelectionOffsets(&selection_start, &selection_end);
  return UTF16ToUnicodeOffsetInText(selection_end);
}

bool AXPlatformNodeAuraLinux::SetCaretOffset(int offset) {
  int character_count =
      atk_text_get_character_count(ATK_TEXT(GetOrCreateAtkObject()));
  if (offset < 0 || offset > character_count)
    offset = character_count;

  // Even if we don't change anything, we still want to act like we
  // were successful.
  if (offset == GetCaretOffset() && !HasSelection())
    return true;

  offset = UnicodeToUTF16OffsetInText(offset);
  if (!SetHypertextSelection(offset, offset))
    return false;

  OnTextSelectionChanged();
  return true;
}

bool AXPlatformNodeAuraLinux::SetTextSelectionForAtkText(int start_offset,
                                                         int end_offset) {
  start_offset = UnicodeToUTF16OffsetInText(start_offset);
  end_offset = UnicodeToUTF16OffsetInText(end_offset);

  base::string16 text = GetHypertext();
  if (start_offset < 0 || start_offset > int{text.length()})
    return false;
  if (end_offset < 0 || end_offset > int{text.length()})
    return false;

  // We must put these in the correct order so that we can do
  // a comparison with the existing start and end below.
  if (end_offset < start_offset)
    std::swap(start_offset, end_offset);

  // Even if we don't change anything, we still want to act like we
  // were successful.
  int old_start_offset, old_end_offset;
  GetSelectionExtents(&old_start_offset, &old_end_offset);
  if (old_start_offset == start_offset && old_end_offset == end_offset)
    return true;

  if (!SetHypertextSelection(start_offset, end_offset))
    return false;

  OnTextSelectionChanged();
  return true;
}

bool AXPlatformNodeAuraLinux::HasSelection() {
  std::pair<int, int> selection;
  GetSelectionOffsets(&selection.first, &selection.second);
  return SelectionOffsetsIndicateSelection(selection);
}

void AXPlatformNodeAuraLinux::GetSelectionExtents(int* start_offset,
                                                  int* end_offset) {
  if (start_offset)
    *start_offset = 0;
  if (end_offset)
    *end_offset = 0;

  int selection_start, selection_end;
  GetSelectionOffsets(&selection_start, &selection_end);
  if (selection_start < 0 || selection_end < 0 ||
      selection_start == selection_end)
    return;

  // We should ignore the direction of the selection when exposing start and
  // end offsets. According to the ATK documentation the end offset is always
  // the offset immediately past the end of the selection. This wouldn't make
  // sense if end < start.
  if (selection_end < selection_start)
    std::swap(selection_start, selection_end);

  selection_start = UTF16ToUnicodeOffsetInText(selection_start);
  selection_end = UTF16ToUnicodeOffsetInText(selection_end);

  if (start_offset)
    *start_offset = selection_start;
  if (end_offset)
    *end_offset = selection_end;
}

// Since this method doesn't return a static gchar*, we expect the caller of
// atk_text_get_selection to free the return value.
gchar* AXPlatformNodeAuraLinux::GetSelectionWithText(int* start_offset,
                                                     int* end_offset) {
  int selection_start, selection_end;
  GetSelectionExtents(&selection_start, &selection_end);
  if (start_offset)
    *start_offset = selection_start;
  if (end_offset)
    *end_offset = selection_end;

  if (selection_start < 0 || selection_end < 0 ||
      selection_start == selection_end)
    return nullptr;

  return atk_text::GetText(ATK_TEXT(GetOrCreateAtkObject()), selection_start,
                           selection_end);
}

bool AXPlatformNodeAuraLinux::IsInLiveRegion() {
  return GetData().HasStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus);
}

#if defined(ATK_230)
void AXPlatformNodeAuraLinux::ScrollToPoint(AtkCoordType atk_coord_type,
                                            int x,
                                            int y) {
  gfx::Point scroll_to(x, y);
  switch (atk_coord_type) {
    case ATK_XY_SCREEN:
      break;
    case ATK_XY_WINDOW:
      scroll_to += GetParentFrameOriginInScreenCoordinates();
      break;
    case ATK_XY_PARENT:
      scroll_to += GetParentOriginInScreenCoordinates();
      break;
  }

  ui::AXActionData action_data;
  action_data.target_node_id = GetData().id;
  action_data.action = ax::mojom::Action::kScrollToPoint;
  action_data.target_point = scroll_to;
  GetDelegate()->AccessibilityPerformAction(action_data);
}
#endif  // defined(ATK_230)

#if defined(ATK_230)
void AXPlatformNodeAuraLinux::ScrollNodeIntoView(
    AtkScrollType atk_scroll_type) {
  gfx::Rect r = GetDelegate()->GetBoundsRect(AXCoordinateSystem::kScreen,
                                             AXClippingBehavior::kUnclipped);
  r -= r.OffsetFromOrigin();

  ui::AXActionData action_data;
  action_data.target_node_id = GetData().id;
  action_data.action = ax::mojom::Action::kScrollToMakeVisible;
  action_data.target_rect = r;

  action_data.horizontal_scroll_alignment = ax::mojom::ScrollAlignment::kNone;
  action_data.vertical_scroll_alignment = ax::mojom::ScrollAlignment::kNone;

  switch (atk_scroll_type) {
    case ATK_SCROLL_TOP_LEFT:
      action_data.vertical_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentTop;
      action_data.horizontal_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentLeft;
      break;
    case ATK_SCROLL_BOTTOM_RIGHT:
      action_data.horizontal_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentRight;
      action_data.vertical_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentBottom;
      break;
    case ATK_SCROLL_TOP_EDGE:
      action_data.vertical_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentTop;
      break;
    case ATK_SCROLL_BOTTOM_EDGE:
      action_data.vertical_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentBottom;
      break;
    case ATK_SCROLL_LEFT_EDGE:
      action_data.horizontal_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentLeft;
      break;
    case ATK_SCROLL_RIGHT_EDGE:
      action_data.horizontal_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentRight;
      break;
    case ATK_SCROLL_ANYWHERE:
      action_data.horizontal_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentClosestEdge;
      action_data.vertical_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentClosestEdge;
      break;
  }

  GetDelegate()->AccessibilityPerformAction(action_data);
}

#endif  // defined(ATK_230)

AtkAttributes AXPlatformNodeAuraLinux::ComputeTextAttributes() const {
  AtkAttributeSet* attributes = nullptr;

  int color;
  if (GetIntAttribute(ax::mojom::IntAttribute::kBackgroundColor, &color)) {
    unsigned int red = SkColorGetR(color);
    unsigned int green = SkColorGetG(color);
    unsigned int blue = SkColorGetB(color);
    std::string color_value = base::NumberToString(red) + ',' +
                              base::NumberToString(green) + ',' +
                              base::NumberToString(blue);
    AddTextAttributeToSet(ATK_TEXT_ATTR_BG_COLOR, color_value, &attributes);
  }

  if (GetIntAttribute(ax::mojom::IntAttribute::kColor, &color)) {
    unsigned int red = SkColorGetR(color);
    unsigned int green = SkColorGetG(color);
    unsigned int blue = SkColorGetB(color);
    std::string color_value = base::NumberToString(red) + ',' +
                              base::NumberToString(green) + ',' +
                              base::NumberToString(blue);
    AddTextAttributeToSet(ATK_TEXT_ATTR_FG_COLOR, color_value, &attributes);
  }

  std::string font_family(
      GetInheritedStringAttribute(ax::mojom::StringAttribute::kFontFamily));
  // Attribute has no default value.
  if (!font_family.empty()) {
    AddTextAttributeToSet(ATK_TEXT_ATTR_FAMILY_NAME, font_family, &attributes);
  }

  float font_size;
  // Attribute has no default value.
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kFontSize, &font_size)) {
    // The ATK Spec requires the value to be in pt, not in pixels.
    // There are 72 points per inch.
    // We assume that there are 96 pixels per inch on a standard display.
    // TODO(nektar): Figure out the current value of pixels per inch.
    float points = font_size * 72.0 / 96.0;
    AddTextAttributeToSet(ATK_TEXT_ATTR_SIZE,
                          base::NumberToString(points) + "pt", &attributes);
  }

  // TODO(nektar): Add Blink support for the following attributes:
  // text-line-through-mode, text-line-through-width, text-outline:false,
  // text-position:baseline, text-shadow:none, text-underline-mode:continuous.

  int32_t text_style = GetIntAttribute(ax::mojom::IntAttribute::kTextStyle);
  if (text_style) {
    if (GetData().HasTextStyle(ax::mojom::TextStyle::kBold)) {
      // TODO(mrobinson): This is based on the weight that CSS uses for "bold,"
      // but we'd like to support more font weights here.
      AddTextAttributeToSet(ATK_TEXT_ATTR_WEIGHT, "700", &attributes);
    }

    if (GetData().HasTextStyle(ax::mojom::TextStyle::kItalic)) {
      // TODO(mrobinson): We'd also like to support "oblique" here.
      AddTextAttributeToSet(ATK_TEXT_ATTR_STYLE, "italic", &attributes);
    }

    if (GetData().HasTextStyle(ax::mojom::TextStyle::kLineThrough))
      AddTextAttributeToSet(ATK_TEXT_ATTR_STRIKETHROUGH, "true", &attributes);

    if (GetData().HasTextStyle(ax::mojom::TextStyle::kUnderline)) {
      // TODO(mrobinson): Figure out a more specific value.
      AddTextAttributeToSet(ATK_TEXT_ATTR_UNDERLINE, "single", &attributes);
    }
  }

  // Screen readers look at the text attributes to determine if something is
  // misspelled, so we need to propagate any spelling attributes from immediate
  // parents of text-only objects. AXPlatformNodeBase::GetInvalidValue takes
  // care of searching upward to find the appropriate values.
  std::string invalid_value = GetInvalidValue();
  if (!invalid_value.empty())
    AddTextAttributeToSet(ATK_TEXT_ATTR_UNDERLINE, "error", &attributes);

  std::string language = GetDelegate()->GetLanguage();
  if (!language.empty())
    AddTextAttributeToSet(ATK_TEXT_ATTR_LANGUAGE, language, &attributes);

  auto text_direction = static_cast<ax::mojom::TextDirection>(
      GetIntAttribute(ax::mojom::IntAttribute::kTextDirection));
  switch (text_direction) {
    case ax::mojom::TextDirection::kNone:
      break;
    case ax::mojom::TextDirection::kLtr:
      AddTextAttributeToSet(ATK_TEXT_ATTR_LANGUAGE, "ltr", &attributes);
      break;
    case ax::mojom::TextDirection::kRtl:
      AddTextAttributeToSet(ATK_TEXT_ATTR_LANGUAGE, "rtl", &attributes);
      break;
    case ax::mojom::TextDirection::kTtb:
      // Not listed in the ATK docs.
      AddTextAttributeToSet(ATK_TEXT_ATTR_LANGUAGE, "ttb", &attributes);
      break;
    case ax::mojom::TextDirection::kBtt:
      // Not listed in the ATK docs.
      AddTextAttributeToSet(ATK_TEXT_ATTR_LANGUAGE, "btt", &attributes);
      break;
  }

  return AtkAttributes(attributes);
}

void AXPlatformNodeAuraLinux::ComputeStylesIfNeeded() {
  if (!offset_to_text_attributes_.empty())
    return;

  default_text_attributes_ = ComputeTextAttributes();
  std::map<int, AtkAttributes> attributes_map;
  if (IsLeaf()) {
    attributes_map[0] =
        AtkAttributes(CopyAttributeSet(default_text_attributes_.get()));
    MergeSpellingIntoAtkTextAttributesAtOffset(0 /* start_offset */,
                                               &attributes_map);
    offset_to_text_attributes_.swap(attributes_map);
    return;
  }

  int start_offset = 0;
  for (auto child_iterator_ptr = GetDelegate()->ChildrenBegin();
       *child_iterator_ptr != *GetDelegate()->ChildrenEnd();
       ++(*child_iterator_ptr)) {
    auto* child = AtkObjectToAXPlatformNodeAuraLinux(
        child_iterator_ptr->GetNativeViewAccessible());
    if (!child)
      continue;

    AtkAttributes attributes = child->ComputeTextAttributes();
    if (attributes_map.empty()) {
      attributes_map[start_offset] = std::move(attributes);
    } else {
      // Only add the attributes for this child if we are at the start of a new
      // style span.
      AtkAttributeSet* previous_attributes =
          attributes_map.rbegin()->second.get();
      if (!AttributeSetsEqual(attributes.get(), previous_attributes))
        attributes_map[start_offset] = std::move(attributes);
    }

    if (child->IsTextOnlyObject()) {
      MergeSpellingIntoAtkTextAttributesAtOffset(start_offset, &attributes_map);
      start_offset += child->GetHypertext().length();
    } else {
      start_offset += 1;
    }
  }

  offset_to_text_attributes_.swap(attributes_map);
}

void AXPlatformNodeAuraLinux::MergeSpellingIntoAtkTextAttributesAtOffset(
    int offset,
    std::map<int, AtkAttributes>* text_attributes) {
  DCHECK(text_attributes);

  int hypertext_length = static_cast<int>(GetHypertext().length());
  std::map<int, AtkAttributeSet*> spelling_attributes;
  if (IsTextOnlyObject()) {
    const std::vector<int32_t>& marker_types =
        GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes);
    const std::vector<int>& marker_starts =
        GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts);
    const std::vector<int>& marker_ends =
        GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds);
    for (size_t i = 0; i < marker_types.size(); ++i) {
      bool isSpellingError =
          (marker_types[i] &
           static_cast<int32_t>(ax::mojom::MarkerType::kSpelling)) != 0;
      bool isGrammarError =
          (marker_types[i] &
           static_cast<int32_t>(ax::mojom::MarkerType::kGrammar)) != 0;
      if (!isSpellingError && !isGrammarError)
        continue;

      // Add a starting attribute for the error, if one does not already exist.
      // We use a default value of an AtkAttributes (unique_ptr of
      // AtkAttributeSet*) set to a nullptr. Since AtkAttributeSet is a GSList,
      // a nullptr is an empty set.
      int marker_start_offset = offset + marker_starts[i];
      auto default_value = std::make_pair(marker_start_offset, nullptr);
      auto iterator = text_attributes->insert(default_value).first;

      if (!HasInvalidAttributeInSet(iterator->second.get())) {
        // `attributes` is a unique_ptr, but here we want to modify the head of
        // the list without having the unique_ptr delete it. We leak the
        // pointer to `attribute_set` and then reset the new list value into
        // the iterator.
        AtkAttributeSet* attribute_set = iterator->second.release();
        AddTextAttributeToSet(ATK_TEXT_ATTR_UNDERLINE, "error", &attribute_set);
        if (isSpellingError) {
          AddTextAttributeToSet(ATK_TEXT_ATTR_INVALID, "spelling",
                                &attribute_set);
        }
        if (isGrammarError) {
          AddTextAttributeToSet(ATK_TEXT_ATTR_INVALID, "grammar",
                                &attribute_set);
        }
        iterator->second.reset(attribute_set);
      }

      // Make sure there is at least an empty attribute set to end the marker.
      int marker_end_offset = offset + marker_ends[i];
      DCHECK_LE(marker_end_offset, hypertext_length);
      text_attributes->insert(std::make_pair(marker_end_offset, nullptr));
    }
  }

  if (IsPlainTextField()) {
    AXPlatformNodeDelegate* delegate = GetDelegate();
    DCHECK(delegate);

    int anchor_start_offset = offset;
    AXNodeRange range(delegate->CreateTextPositionAt(0),
                      delegate->CreateTextPositionAt(hypertext_length));
    for (const auto& leaf_text_range : range) {
      DCHECK(!leaf_text_range.IsNull());
      DCHECK_EQ(leaf_text_range.anchor()->GetAnchor(),
                leaf_text_range.focus()->GetAnchor())
          << "An leaf text range should only span a single object.";
      auto* node = static_cast<AXPlatformNodeAuraLinux*>(
          delegate->GetFromNodeID(leaf_text_range.anchor()->GetAnchor()->id()));
      node->MergeSpellingIntoAtkTextAttributesAtOffset(anchor_start_offset,
                                                       text_attributes);
      anchor_start_offset += leaf_text_range.GetText().length();
    }
  }
}

int AXPlatformNodeAuraLinux::FindStartOfStyle(
    int start_offset,
    ui::TextBoundaryDirection direction) {
  int text_length = GetHypertext().length();
  DCHECK_GE(start_offset, 0);
  DCHECK_LE(start_offset, text_length);
  DCHECK(!offset_to_text_attributes_.empty());

  switch (direction) {
    case ui::BACKWARDS_DIRECTION: {
      auto iterator = offset_to_text_attributes_.upper_bound(start_offset);
      --iterator;
      return iterator->first;
    }
    case ui::FORWARDS_DIRECTION: {
      const auto iterator =
          offset_to_text_attributes_.upper_bound(start_offset);
      if (iterator == offset_to_text_attributes_.end())
        return text_length;
      return iterator->first;
    }
  }

  NOTREACHED();
  return start_offset;
}

AtkAttributeSet* AXPlatformNodeAuraLinux::GetTextAttributes(int offset,
                                                            int* start_offset,
                                                            int* end_offset) {
  ComputeStylesIfNeeded();
  DCHECK(!offset_to_text_attributes_.empty());

  int utf16_offset = UnicodeToUTF16OffsetInText(offset);
  int style_start = FindStartOfStyle(utf16_offset, ui::BACKWARDS_DIRECTION);
  int style_end = FindStartOfStyle(utf16_offset, ui::FORWARDS_DIRECTION);

  auto iterator = offset_to_text_attributes_.find(style_start);
  DCHECK(iterator != offset_to_text_attributes_.end());

  SetIntPointerValueIfNotNull(start_offset,
                              UTF16ToUnicodeOffsetInText(style_start));
  SetIntPointerValueIfNotNull(end_offset,
                              UTF16ToUnicodeOffsetInText(style_end));
  return iterator->second.get();
}

AtkAttributeSet* AXPlatformNodeAuraLinux::GetDefaultTextAttributes() {
  ComputeStylesIfNeeded();
  return default_text_attributes_.get();
}

}  // namespace ui
