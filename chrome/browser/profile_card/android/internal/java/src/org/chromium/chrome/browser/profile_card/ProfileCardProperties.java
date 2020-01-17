// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card.internal;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class ProfileCardProperties extends PropertyModel {
    public static final PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<String>();
    public static final PropertyModel.WritableObjectPropertyKey<String> DESCRIPTION =
            new PropertyModel.WritableObjectPropertyKey<String>();
    public static final PropertyModel.WritableBooleanPropertyKey IS_DIALOG_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {TITLE, DESCRIPTION, IS_DIALOG_VISIBLE};
}
