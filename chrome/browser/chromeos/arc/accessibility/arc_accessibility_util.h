// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_UTIL_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/optional.h"
#include "components/arc/mojom/accessibility_helper.mojom-forward.h"
#include "ui/accessibility/ax_enum_util.h"

namespace arc {
class AccessibilityInfoDataWrapper;

ax::mojom::Event ToAXEvent(mojom::AccessibilityEventType arc_event_type,
                           AccessibilityInfoDataWrapper* source_node,
                           AccessibilityInfoDataWrapper* focused_node);

base::Optional<mojom::AccessibilityActionType> ConvertToAndroidAction(
    ax::mojom::Action action);

std::string ToLiveStatusString(mojom::AccessibilityLiveRegionType type);

bool IsImportantInAndroid(mojom::AccessibilityNodeInfoData* node);

bool HasImportantProperty(mojom::AccessibilityNodeInfoData* node);

bool HasStandardAction(mojom::AccessibilityNodeInfoData* node,
                       mojom::AccessibilityActionType action);

template <class DataType, class PropType>
bool GetBooleanProperty(DataType* node, PropType prop) {
  if (!node->boolean_properties)
    return false;

  auto it = node->boolean_properties->find(prop);
  if (it == node->boolean_properties->end())
    return false;

  return it->second;
}

template <class PropMTypeMap, class PropType>
bool HasProperty(PropMTypeMap properties, PropType prop) {
  if (!properties)
    return false;

  return properties->find(prop) != properties->end();
}

template <class PropMTypeMap, class PropType, class OutType>
bool GetProperty(PropMTypeMap properties, PropType prop, OutType* out_value) {
  if (!properties)
    return false;

  auto it = properties->find(prop);
  if (it == properties->end())
    return false;

  *out_value = it->second;
  return true;
}

template <class InfoDataType, class PropType>
bool HasNonEmptyStringProperty(InfoDataType* node, PropType prop) {
  if (!node || !node->string_properties)
    return false;

  auto it = node->string_properties->find(prop);
  if (it == node->string_properties->end())
    return false;

  return !it->second.empty();
}

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_UTIL_H_
