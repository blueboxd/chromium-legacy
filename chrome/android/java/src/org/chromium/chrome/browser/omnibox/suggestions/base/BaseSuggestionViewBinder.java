// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.support.v4.view.ViewCompat;

import androidx.annotation.CallSuper;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * Binds base suggestion view properties.
 */
public class BaseSuggestionViewBinder
        implements ViewBinder<PropertyModel, BaseSuggestionView, PropertyKey> {
    @Override
    @CallSuper
    public void bind(PropertyModel model, BaseSuggestionView view, PropertyKey propertyKey) {
        if (BaseSuggestionViewProperties.SUGGESTION_DELEGATE == propertyKey) {
            view.setDelegate(model.get(BaseSuggestionViewProperties.SUGGESTION_DELEGATE));
        } else if (BaseSuggestionViewProperties.REFINE_VISIBLE == propertyKey) {
            view.setRefineVisible(model.get(BaseSuggestionViewProperties.REFINE_VISIBLE));
        } else if (SuggestionCommonProperties.LAYOUT_DIRECTION == propertyKey) {
            ViewCompat.setLayoutDirection(
                    view, model.get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        } else if (SuggestionCommonProperties.USE_DARK_COLORS == propertyKey) {
            boolean useDarkColors = model.get(SuggestionCommonProperties.USE_DARK_COLORS);
            int tint = ColorUtils.getIconTint(view.getContext(), !useDarkColors).getDefaultColor();
            view.updateIconTint(tint);
        }
    }
}
