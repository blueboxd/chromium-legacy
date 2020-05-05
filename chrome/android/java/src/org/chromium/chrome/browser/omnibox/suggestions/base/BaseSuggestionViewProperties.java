// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The base set of properties for most omnibox suggestions. */
public class BaseSuggestionViewProperties {
    /** Describes density of the suggestions. */
    @IntDef({Density.COMFORTABLE, Density.SEMICOMPACT, Density.COMPACT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Density {
        int COMFORTABLE = 0;
        int SEMICOMPACT = 1;
        int COMPACT = 2;
    }

    /** SuggestionDrawableState to show as a suggestion icon. */
    public static final WritableObjectPropertyKey<SuggestionDrawableState> ICON =
            new WritableObjectPropertyKey<>();

    /** SuggestionDrawableState to show as an action icon. */
    public static final WritableObjectPropertyKey<SuggestionDrawableState> ACTION_ICON =
            new WritableObjectPropertyKey<>();

    /** ActionCallback to invoke when user presses the ActionIcon. */
    public static final WritableObjectPropertyKey<Runnable> ACTION_CALLBACK =
            new WritableObjectPropertyKey<>();

    /** Delegate receiving user events. */
    public static final WritableObjectPropertyKey<SuggestionViewDelegate> SUGGESTION_DELEGATE =
            new WritableObjectPropertyKey<>();

    /** Specifies how densely suggestions should be packed. */
    public static final WritableIntPropertyKey DENSITY = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {ICON, DENSITY, ACTION_ICON, ACTION_CALLBACK, SUGGESTION_DELEGATE};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
