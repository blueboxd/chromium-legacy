// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of the bottom bar properties. */
class BottomBarProperties {
    public static interface OnClickListener {
        /**
         * Called when clicks on the home button.
         * */
        void onHomeButtonClicked();

        /**
         * Called when clicks on the explore button.
         * */
        void onExploreButtonClicked();
    }
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<OnClickListener> ON_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<OnClickListener>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {IS_INCOGNITO, IS_VISIBLE, ON_CLICK_LISTENER};
}