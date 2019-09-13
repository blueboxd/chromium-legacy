// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.graphics.Bitmap;
import android.text.Spannable;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The properties associated with rendering the answer suggestion view.
 */
class AnswerSuggestionViewProperties {
    @IntDef({AnswerIcon.UNDEFINED, AnswerIcon.CALCULATOR, AnswerIcon.DICTIONARY, AnswerIcon.FINANCE,
            AnswerIcon.KNOWLEDGE, AnswerIcon.SUNRISE, AnswerIcon.TRANSLATION, AnswerIcon.WEATHER,
            AnswerIcon.EVENT, AnswerIcon.CURRENCY, AnswerIcon.SPORTS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AnswerIcon {
        int UNDEFINED = 0;
        int CALCULATOR = 1;
        int DICTIONARY = 2;
        int FINANCE = 3;
        int KNOWLEDGE = 4;
        int SUNRISE = 5;
        int TRANSLATION = 6;
        int WEATHER = 7;
        int EVENT = 8;
        int CURRENCY = 9;
        int SPORTS = 10;
    }

    /** The delegate to handle actions on the suggestion view. */
    public static final WritableObjectPropertyKey<SuggestionViewDelegate> DELEGATE =
            new WritableObjectPropertyKey<>();
    /** The answer image to be shown. */
    public static final WritableObjectPropertyKey<Bitmap> ANSWER_IMAGE =
            new WritableObjectPropertyKey<>();
    /** The suggestion icon type shown. */
    public static final WritableIntPropertyKey ANSWER_ICON_TYPE = new WritableIntPropertyKey();

    /** The sizing information for the first line of text. */
    public static final WritableIntPropertyKey TEXT_LINE_1_SIZE = new WritableIntPropertyKey();
    /** The maximum number of lines to be shown for the first line of text. */
    public static final WritableIntPropertyKey TEXT_LINE_1_MAX_LINES = new WritableIntPropertyKey();
    /** The actual text content for the first line of text. */
    public static final WritableObjectPropertyKey<Spannable> TEXT_LINE_1_TEXT =
            new WritableObjectPropertyKey<>();
    /** The accessibility description to be announced with this line. */
    public static final WritableObjectPropertyKey<String> TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** The sizing information for the second line of text. */
    public static final WritableIntPropertyKey TEXT_LINE_2_SIZE = new WritableIntPropertyKey();
    /** The maximum number of lines to be shown for the second line of text. */
    public static final WritableIntPropertyKey TEXT_LINE_2_MAX_LINES = new WritableIntPropertyKey();
    /** The actual text content for the second line of text. */
    public static final WritableObjectPropertyKey<Spannable> TEXT_LINE_2_TEXT =
            new WritableObjectPropertyKey<>();
    /** The accessibility description to be announced with this line. */
    public static final WritableObjectPropertyKey<String> TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_UNIQUE_KEYS = new PropertyKey[] {DELEGATE, ANSWER_IMAGE,
            ANSWER_ICON_TYPE, TEXT_LINE_1_SIZE, TEXT_LINE_1_MAX_LINES, TEXT_LINE_1_TEXT,
            TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION, TEXT_LINE_2_SIZE, TEXT_LINE_2_MAX_LINES,
            TEXT_LINE_2_TEXT, TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
