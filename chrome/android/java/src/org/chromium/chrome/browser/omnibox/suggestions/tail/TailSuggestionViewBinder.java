// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.tail;

import android.support.annotation.ColorRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties associated with the tail suggestion view. */
public class TailSuggestionViewBinder extends BaseSuggestionViewBinder {
    /** @see BaseSuggestionViewBinder#bind(PropertyModel, BaseSuggestionView, PropertyKey) */
    @Override
    public void bind(PropertyModel model, BaseSuggestionView view, PropertyKey propertyKey) {
        super.bind(model, view, propertyKey);
        final TailSuggestionView tailView = (TailSuggestionView) view.getContentView();

        if (TailSuggestionViewProperties.ALIGNMENT_MANAGER == propertyKey) {
            tailView.setAlignmentManager(model.get(TailSuggestionViewProperties.ALIGNMENT_MANAGER));
        } else if (propertyKey == TailSuggestionViewProperties.TEXT) {
            tailView.setTailText(model.get(TailSuggestionViewProperties.TEXT));
        } else if (propertyKey == TailSuggestionViewProperties.FILL_INTO_EDIT) {
            tailView.setFullText(model.get(TailSuggestionViewProperties.FILL_INTO_EDIT));
        } else if (propertyKey == SuggestionCommonProperties.USE_DARK_COLORS) {
            final boolean useDarkMode = model.get(SuggestionCommonProperties.USE_DARK_COLORS);
            @ColorRes
            final int color = useDarkMode ? R.color.default_text_color_dark
                                          : R.color.default_text_color_light;
            tailView.setTextColor(ApiCompatibilityUtils.getColor(view.getResources(), color));
        }
    }
}
