// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_OOBE_DIALOG_SIZE_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_OOBE_DIALOG_SIZE_UTILS_H_

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos {

// Position OOBE dialog according to specs inside |host_bounds| excluding shelf.
// |host_bounds| is in coordinates of oobe dialog widget. |result| is
// in the same coordinates of |host_bounds|.
void CalculateOobeDialogBounds(const gfx::Rect& host_bounds,
                               int shelf_height,
                               gfx::Rect* result);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_OOBE_DIALOG_SIZE_UTILS_H_
