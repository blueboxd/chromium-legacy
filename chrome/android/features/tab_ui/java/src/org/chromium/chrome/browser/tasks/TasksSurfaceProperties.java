// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.MostVisitedListProperties.IS_VISIBLE;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of the tasks surface properties. */
public class TasksSurfaceProperties {
    private TasksSurfaceProperties() {}

    public static final PropertyModel.WritableBooleanPropertyKey IS_FAKE_SEARCH_BOX_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_TAB_CAROUSEL =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableBooleanPropertyKey IS_VOICE_RECOGNITION_BUTTON_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> FAKE_SEARCH_BOX_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<View.OnClickListener>();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> MORE_TABS_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<View.OnClickListener>();
    public static final PropertyModel.WritableBooleanPropertyKey MV_TILES_VISIBLE = IS_VISIBLE;
    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {IS_FAKE_SEARCH_BOX_VISIBLE,
            IS_TAB_CAROUSEL, IS_VOICE_RECOGNITION_BUTTON_VISIBLE, FAKE_SEARCH_BOX_CLICK_LISTENER,
            MORE_TABS_CLICK_LISTENER, MV_TILES_VISIBLE};
}
