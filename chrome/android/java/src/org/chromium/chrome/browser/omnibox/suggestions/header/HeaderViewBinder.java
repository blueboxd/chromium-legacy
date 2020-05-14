// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import androidx.core.view.ViewCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties associated with the header suggestion view. */
public class HeaderViewBinder {
    /** @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object) */
    public static void bind(PropertyModel model, HeaderView view, PropertyKey propertyKey) {
        if (HeaderViewProperties.TITLE == propertyKey) {
            view.getTextView().setText(model.get(HeaderViewProperties.TITLE));
        } else if (propertyKey == SuggestionCommonProperties.USE_DARK_COLORS) {
            final boolean useDarkColors = model.get(SuggestionCommonProperties.USE_DARK_COLORS);
            view.getTextView().setTextAppearance(
                    ChromeColors.getMediumTextSecondaryStyle(!useDarkColors));
            ApiCompatibilityUtils.setImageTintList(view.getIconView(),
                    ChromeColors.getPrimaryIconTint(view.getContext(), !useDarkColors));
        } else if (propertyKey == SuggestionCommonProperties.LAYOUT_DIRECTION) {
            ViewCompat.setLayoutDirection(
                    view, model.get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        } else if (propertyKey == HeaderViewProperties.IS_EXPANDED) {
            boolean isExpanded = model.get(HeaderViewProperties.IS_EXPANDED);
            view.getIconView().setImageResource(isExpanded ? R.drawable.ic_expand_less_black_24dp
                                                           : R.drawable.ic_expand_more_black_24dp);
            view.setExpandedStateForAccessibility(isExpanded);
        } else if (propertyKey == HeaderViewProperties.DELEGATE) {
            Runnable delegate = model.get(HeaderViewProperties.DELEGATE);
            if (delegate != null) {
                view.setOnClickListener(v -> delegate.run());
            } else {
                view.setOnClickListener(null);
            }
        }
    }
}
