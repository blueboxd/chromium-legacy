// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.support.v4.view.ViewCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionViewProperties.AnswerIcon;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A mechanism binding AnswerSuggestion properties to its view. */
public class AnswerSuggestionViewBinder {
    /** @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object) */
    public static void bind(
            PropertyModel model, AnswerSuggestionView view, PropertyKey propertyKey) {
        if (AnswerSuggestionViewProperties.DELEGATE.equals(propertyKey)) {
            view.setDelegate(model.get(AnswerSuggestionViewProperties.DELEGATE));
        } else if (SuggestionCommonProperties.USE_DARK_COLORS.equals(propertyKey)) {
            view.setUseDarkColors(model.get(SuggestionCommonProperties.USE_DARK_COLORS));
        } else if (AnswerSuggestionViewProperties.ANSWER_IMAGE.equals(propertyKey)) {
            view.setIconBitmap(model.get(AnswerSuggestionViewProperties.ANSWER_IMAGE));
        } else if (AnswerSuggestionViewProperties.ANSWER_ICON_TYPE.equals(propertyKey)) {
            int type = model.get(AnswerSuggestionViewProperties.ANSWER_ICON_TYPE);
            if (type == AnswerIcon.UNDEFINED) return;
            view.setFallbackIconRes(getAnswerIcon(type));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT.equals(propertyKey)) {
            view.setLine1TextContent(model.get(AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT.equals(propertyKey)) {
            view.setLine2TextContent(model.get(AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION.equals(
                           propertyKey)) {
            view.setLine1AccessibilityDescription(model.get(
                    AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION.equals(
                           propertyKey)) {
            view.setLine2AccessibilityDescription(model.get(
                    AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION));
        } else if (SuggestionCommonProperties.LAYOUT_DIRECTION.equals(propertyKey)) {
            ViewCompat.setLayoutDirection(
                    view, model.get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        }
    }

    /**
     * Convert AnswerIcon type to drawable resource type representing answer icon.
     *
     * Answers are not shown when user is in incognito mode, so we can rely on
     * configuration UI to define colors for light and dark mode that will be used
     * by the icons below.
     *
     * @param type AnswerIcon type to get drawable for.
     */
    private static final int getAnswerIcon(@AnswerIcon int type) {
        switch (type) {
            case AnswerIcon.CALCULATOR:
                return R.drawable.ic_equals_sign_round;
            case AnswerIcon.DICTIONARY:
                return R.drawable.ic_book_round;
            case AnswerIcon.FINANCE:
                return R.drawable.ic_swap_vert_round;
            case AnswerIcon.KNOWLEDGE:
                return R.drawable.ic_google_round;
            case AnswerIcon.SUNRISE:
                return R.drawable.ic_wb_sunny_round;
            case AnswerIcon.TRANSLATION:
                return R.drawable.logo_translate_round;
            case AnswerIcon.WEATHER:
                return R.drawable.logo_partly_cloudy;
            case AnswerIcon.EVENT:
                return R.drawable.ic_event_round;
            case AnswerIcon.CURRENCY:
                return R.drawable.ic_loop_round;
            case AnswerIcon.SPORTS:
                return R.drawable.ic_google_round;
            default:
                assert false : "Invalid answer type: " + type;
                return 0;
        }
    }
}
