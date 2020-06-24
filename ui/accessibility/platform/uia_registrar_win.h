// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_UIA_REGISTRAR_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_UIA_REGISTRAR_WIN_H_

#include <objbase.h>
#include <uiautomation.h>
#include "base/macros.h"
#include "base/no_destructor.h"
#include "ui/accessibility/ax_export.h"

namespace ui {
// {cc7eeb32-4b62-4f4c-aff6-1c2e5752ad8e}
static const GUID kUiaPropertyUniqueIdGuid = {
    0xcc7eeb32,
    0x4b62,
    0x4f4c,
    {0xaf, 0xf6, 0x1c, 0x2e, 0x57, 0x52, 0xad, 0x8e}};

class AX_EXPORT UiaRegistrarWin {
 public:
  UiaRegistrarWin();
  ~UiaRegistrarWin();
  PROPERTYID GetUiaUniqueIdPropertyId() const;

  static const UiaRegistrarWin& GetInstance();

 private:
  PROPERTYID uia_unique_id_property_id_ = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_UIA_REGISTRAR_WIN_H_
