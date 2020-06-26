// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.TrendyTermsProperties.TRENDY_TERM_ICON_DRAWABLE_ID;
import static org.chromium.chrome.browser.tasks.TrendyTermsProperties.TRENDY_TERM_ICON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TrendyTermsProperties.TRENDY_TERM_STRING;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChipView;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

/**
 * ViewBinder for trendy terms.
 */
public class TrendyTermsViewBinder {
    public static void bind(
            PropertyModel model, ViewLookupCachingFrameLayout view, PropertyKey propertyKey) {
        ChipView chipView = (ChipView) view.fastFindViewById(R.id.trendy_term_chip);
        if (propertyKey == TRENDY_TERM_STRING) {
            chipView.getPrimaryTextView().setText(model.get(TRENDY_TERM_STRING));
        } else if (propertyKey == TRENDY_TERM_ICON_DRAWABLE_ID) {
            int iconDrawableId = model.get(TRENDY_TERM_ICON_DRAWABLE_ID);
            chipView.setIcon(iconDrawableId, true);
        } else if (propertyKey == TRENDY_TERM_ICON_ON_CLICK_LISTENER) {
            chipView.setOnClickListener(model.get(TRENDY_TERM_ICON_ON_CLICK_LISTENER));
        }
    }
}
