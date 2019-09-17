// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of the tasks surface properties. */
public class TasksSurfaceProperties {
    public static final PropertyModel.WritableBooleanPropertyKey IS_TAB_CAROUSEL =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> MORE_TABS_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<View.OnClickListener>();
    public static final PropertyModel.WritableBooleanPropertyKey MV_TILES_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey TOP_PADDING =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {
            IS_TAB_CAROUSEL, MORE_TABS_CLICK_LISTENER, MV_TILES_VISIBLE, TOP_PADDING};
}
