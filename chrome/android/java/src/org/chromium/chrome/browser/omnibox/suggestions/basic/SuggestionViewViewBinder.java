// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.support.annotation.ColorRes;
import android.text.Spannable;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties.SuggestionTextContainer;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties associated with the basic suggestion view. */
public class SuggestionViewViewBinder extends BaseSuggestionViewBinder {
    /** @see BaseSuggestionViewBinder#bind(PropertyModel, BaseSuggestionView, PropertyKey) */
    @Override
    public void bind(PropertyModel model, BaseSuggestionView view, PropertyKey propertyKey) {
        super.bind(model, view, propertyKey);

        if (BaseSuggestionViewProperties.SUGGESTION_DELEGATE == propertyKey) {
            SuggestionView content = (SuggestionView) view.getContentView();
            content.setDelegate(model.get(BaseSuggestionViewProperties.SUGGESTION_DELEGATE));
        } else if (propertyKey == SuggestionViewProperties.TEXT_LINE_1_TEXT) {
            TextView tv = view.findContentView(R.id.line_1);
            final SuggestionTextContainer container =
                    model.get(SuggestionViewProperties.TEXT_LINE_1_TEXT);
            final Spannable span = container != null ? container.text : null;
            tv.setText(span);
            // Force layout to ensure infinite suggest aligns correctly.
            // The view may be re-used and may not require re-positioning by itself.
            // We want to make sure that regardless whether it's a regular text suggestion or
            // infinite suggest, the position is always accurate.
            // Without this change, re-layout will not occur for already measured/laid out
            // suggestion views. This optimizes behavior of all other suggestion types,
            // but penalizes tail suggestions.
            @OmniboxSuggestionType
            int type = model.get(SuggestionViewProperties.SUGGESTION_TYPE);
            if (type == OmniboxSuggestionType.SEARCH_SUGGEST_TAIL) {
                tv.requestLayout();
            }
        } else if (propertyKey == SuggestionViewProperties.SUGGESTION_TYPE) {
            @OmniboxSuggestionType
            int type = model.get(SuggestionViewProperties.SUGGESTION_TYPE);
            if (type == OmniboxSuggestionType.SEARCH_SUGGEST_TAIL) {
                TextView tv = view.findContentView(R.id.line_1);
                tv.requestLayout();
            }
        } else if (propertyKey == SuggestionCommonProperties.USE_DARK_COLORS) {
            updateSuggestionTextColor(view, model);
        } else if (propertyKey == SuggestionViewProperties.IS_SEARCH_SUGGESTION) {
            updateSuggestionTextColor(view, model);
            // https://crbug.com/609680: ensure URLs are always composed LTR and that their
            // components are not re-ordered.
            final boolean isSearch = model.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION);
            final TextView tv = view.findContentView(R.id.line_2);
            tv.setTextDirection(
                    isSearch ? TextView.TEXT_DIRECTION_INHERIT : TextView.TEXT_DIRECTION_LTR);
        } else if (propertyKey == SuggestionViewProperties.TEXT_LINE_2_TEXT) {
            TextView tv = view.findContentView(R.id.line_2);
            final SuggestionTextContainer container =
                    model.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
            final Spannable span = container != null ? container.text : null;
            if (!TextUtils.isEmpty(span)) {
                tv.setText(span);
                tv.setVisibility(View.VISIBLE);
            } else {
                tv.setVisibility(View.GONE);
            }
        }
    }

    private static void updateSuggestionTextColor(BaseSuggestionView view, PropertyModel model) {
        final boolean isSearch = model.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION);
        final boolean useDarkMode = model.get(SuggestionCommonProperties.USE_DARK_COLORS);
        final TextView line1 = view.findContentView(R.id.line_1);
        final TextView line2 = view.findContentView(R.id.line_2);

        @ColorRes
        final int color1 =
                useDarkMode ? R.color.default_text_color_dark : R.color.default_text_color_light;
        line1.setTextColor(ApiCompatibilityUtils.getColor(view.getResources(), color1));

        @ColorRes
        final int color2 = isSearch ? (useDarkMode ? R.color.default_text_color_dark_secondary
                                                   : R.color.default_text_color_light)
                                    : (useDarkMode ? R.color.suggestion_url_dark_modern
                                                   : R.color.suggestion_url_light_modern);
        line2.setTextColor(ApiCompatibilityUtils.getColor(view.getResources(), color2));
    }
}
