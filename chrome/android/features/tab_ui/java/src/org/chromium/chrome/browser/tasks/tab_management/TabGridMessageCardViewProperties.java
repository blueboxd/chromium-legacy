// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * List of properties used by TabGridSecondaryItem.
 */
class TabGridMessageCardViewProperties {
    public static final PropertyModel.WritableObjectPropertyKey<String> ACTION_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String> DESCRIPTION_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String> DESCRIPTION_TEXT_TEMPLATE =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {ACTION_TEXT, DESCRIPTION_TEXT, DESCRIPTION_TEXT_TEMPLATE};
}