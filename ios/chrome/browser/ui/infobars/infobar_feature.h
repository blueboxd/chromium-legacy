// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_FEATURE_H_

#include "base/feature_list.h"

// Feature to choose whether to use the new Messages Infobar design, or the
// legacy one.
// Use IsInfobarUIRebootEnabled() instead of this constant directly.
extern const base::Feature kInfobarUIReboot;

// Feature to choose whether Confirm Infobars use the new Messages UI or the
// legacy one. In order for it to work kInfobarUIReboot also needs to be
// enabled.
// Use IsConfirmInfobarMessagesUIEnabled() instead of this constant directly.
extern const base::Feature kConfirmInfobarMessagesUI;

// Whether the Messages Infobar UI is enabled.
bool IsInfobarUIRebootEnabled();

// Whether the Confirm Infobar Messages UI is enabled.
bool IsConfirmInfobarMessagesUIEnabled();

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_FEATURE_H_
