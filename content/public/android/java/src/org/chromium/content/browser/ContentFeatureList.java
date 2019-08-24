// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

/**
 * Java accessor for base/feature_list.h state.
 */
@JNINamespace("content::android")
@MainDex
public abstract class ContentFeatureList {
    // Prevent instantiation.
    private ContentFeatureList() {}

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in content/browser/android/content_feature_list.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        return ContentFeatureListJni.get().isEnabled(featureName);
    }

    // Alphabetical:
    public static final String BACKGROUND_MEDIA_RENDERER_HAS_MODERATE_BINDING =
            "BackgroundMediaRendererHasModerateBinding";

    public static final String SERVICE_GROUP_IMPORTANCE = "ServiceGroupImportance";

    @NativeMethods
    interface Natives {
        boolean isEnabled(String featureName);
    }
}
