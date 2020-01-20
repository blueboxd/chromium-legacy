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

ax::mojom::Event ToAXEvent(mojom::AccessibilityEventType arc_event_type,
                           mojom::AccessibilityNodeInfoData* node_info_data);

base::Optional<mojom::AccessibilityActionType> ConvertToAndroidAction(
    ax::mojom::Action action);

bool IsImportantInAndroid(mojom::AccessibilityNodeInfoData* node);

bool HasImportantProperty(mojom::AccessibilityNodeInfoData* node);

// TODO(hirokisato) clean up GetProperty methods in AccessibilityNodeInfoData
// and AccessibilityWindowInfoData.
bool GetBooleanProperty(mojom::AccessibilityNodeInfoData* node,
                        mojom::AccessibilityBooleanProperty prop);

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
