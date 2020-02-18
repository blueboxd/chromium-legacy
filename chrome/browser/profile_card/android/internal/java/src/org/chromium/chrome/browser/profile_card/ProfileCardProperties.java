// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import android.graphics.Bitmap;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;

class ProfileCardProperties extends PropertyModel {
    public static final PropertyModel.WritableObjectPropertyKey<Bitmap> AVATAR_BITMAP =
            new PropertyModel.WritableObjectPropertyKey<Bitmap>();
    public static final PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<String>();
    public static final PropertyModel.WritableObjectPropertyKey<String> DESCRIPTION =
            new PropertyModel.WritableObjectPropertyKey<String>();
    public static final PropertyModel.WritableObjectPropertyKey<String> POST_FREQUENCY =
            new PropertyModel.WritableObjectPropertyKey<String>();
    public static final PropertyModel
            .WritableObjectPropertyKey<ArrayList<ContentPreviewPostData>> POST_DATA_LIST =
            new PropertyModel.WritableObjectPropertyKey<ArrayList<ContentPreviewPostData>>();
    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {
            AVATAR_BITMAP, TITLE, DESCRIPTION, POST_FREQUENCY, POST_DATA_LIST, IS_VISIBLE};
}
