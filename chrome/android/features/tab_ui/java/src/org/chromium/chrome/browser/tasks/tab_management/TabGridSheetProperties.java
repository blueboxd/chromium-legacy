// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.view.View.OnClickListener;

import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class TabGridSheetProperties {
    public static final PropertyModel
            .WritableObjectPropertyKey<OnClickListener> COLLAPSE_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<OnClickListener>();
    public static final PropertyModel
            .WritableObjectPropertyKey<OnClickListener> ADD_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<OnClickListener>();
    public static final PropertyModel.WritableObjectPropertyKey<String> HEADER_TITLE =
            new PropertyModel.WritableObjectPropertyKey<String>();
    public static final PropertyModel.WritableIntPropertyKey CONTENT_TOP_MARGIN =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey PRIMARY_COLOR =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<ColorStateList> TINT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey IS_DIALOG_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableObjectPropertyKey<ScrimView.ScrimObserver> SCRIMVIEW_OBSERVER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<TabGridDialogParent.AnimationParams> ANIMATION_PARAMS =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey UNGROUP_BAR_STATUS =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {COLLAPSE_CLICK_LISTENER,
            ADD_CLICK_LISTENER, HEADER_TITLE, CONTENT_TOP_MARGIN, PRIMARY_COLOR, TINT,
            IS_DIALOG_VISIBLE, SCRIMVIEW_OBSERVER, ANIMATION_PARAMS, UNGROUP_BAR_STATUS};
}
