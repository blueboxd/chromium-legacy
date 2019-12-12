// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.text.Spannable;
import android.text.TextUtils;
import android.text.style.UpdateAppearance;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The properties associated with rendering the default suggestion view.
 */
public class SuggestionViewProperties {
    @IntDef({SuggestionIcon.UNSET, SuggestionIcon.BOOKMARK, SuggestionIcon.HISTORY,
            SuggestionIcon.GLOBE, SuggestionIcon.MAGNIFIER, SuggestionIcon.VOICE,
            SuggestionIcon.CALCULATOR, SuggestionIcon.FAVICON, SuggestionIcon.TOTAL_COUNT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SuggestionIcon {
        // This enum is used to back UMA histograms, and should therefore be treated as append-only.
        // See http://cs.chromium.org/SuggestionIconOrFaviconType
        int UNSET = 0;
        int BOOKMARK = 1;
        int HISTORY = 2;
        int GLOBE = 3;
        int MAGNIFIER = 4;
        int VOICE = 5;
        int CALCULATOR = 6;
        int FAVICON = 7;
        int TOTAL_COUNT = 8;
    }

    /**
     * Container for suggestion text that prevents updates when the text/spans has not changed.
     */
    public static class SuggestionTextContainer {
        public final Spannable text;

        public SuggestionTextContainer(Spannable text) {
            this.text = text;
        }

        @Override
        public String toString() {
            return "SuggestionTextContainer: " + (text == null ? "null" : text.toString());
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof SuggestionTextContainer)) return false;
            SuggestionTextContainer other = (SuggestionTextContainer) obj;
            if (!TextUtils.equals(text, other.text)) return false;
            if (text == null) return false;

            UpdateAppearance[] thisSpans = text.getSpans(0, text.length(), UpdateAppearance.class);
            UpdateAppearance[] otherSpans =
                    other.text.getSpans(0, other.text.length(), UpdateAppearance.class);
            if (thisSpans.length != otherSpans.length) return false;
            for (int i = 0; i < thisSpans.length; i++) {
                UpdateAppearance thisSpan = thisSpans[i];
                UpdateAppearance otherSpan = otherSpans[i];
                if (!thisSpan.getClass().equals(otherSpan.getClass())) return false;

                if (text.getSpanStart(thisSpan) != other.text.getSpanStart(otherSpan)
                        || text.getSpanEnd(thisSpan) != other.text.getSpanEnd(otherSpan)
                        || text.getSpanFlags(thisSpan) != other.text.getSpanFlags(otherSpan)) {
                    return false;
                }

                // TODO(tedchoc): This is a dangerous assumption.  We should actually update all
                //                span types we use in suggestion text to implement .equals and
                //                ensure the internal styles (e.g. color used in a foreground span)
                //                is actually the same.  This "seems" safe for now, but not
                //                particularly robust.
                //
                //                Once that happens, share this logic with
                //                UrlBarMediator#isNewTextEquivalentToExistingText.
            }
            return true;
        }
    }

    /** @see OmniboxSuggestionType */
    public static final WritableIntPropertyKey SUGGESTION_TYPE = new WritableIntPropertyKey();

    /** The suggestion icon type shown. @see SuggestionIcon. Used for metric collection purposes. */
    public static final WritableIntPropertyKey SUGGESTION_ICON_TYPE = new WritableIntPropertyKey();

    /** Whether suggestion is a search suggestion. */
    public static final WritableBooleanPropertyKey IS_SEARCH_SUGGESTION =
            new WritableBooleanPropertyKey();

    /** The actual text content for the first line of text. */
    public static final WritableObjectPropertyKey<SuggestionTextContainer> TEXT_LINE_1_TEXT =
            new WritableObjectPropertyKey<>();

    /** The actual text content for the second line of text. */
    public static final WritableObjectPropertyKey<SuggestionTextContainer> TEXT_LINE_2_TEXT =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_UNIQUE_KEYS = new PropertyKey[] {SUGGESTION_TYPE,
            IS_SEARCH_SUGGESTION, SUGGESTION_ICON_TYPE, TEXT_LINE_1_TEXT, TEXT_LINE_2_TEXT};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, BaseSuggestionViewProperties.ALL_KEYS);
}
