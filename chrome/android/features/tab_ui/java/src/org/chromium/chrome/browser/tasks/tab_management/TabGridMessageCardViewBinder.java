// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.ViewGroup;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for TabGridSecondaryItem.
 */
class TabGridMessageCardViewBinder {
    public static void bind(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        assert view instanceof TabGridMessageCardView;

        TabGridMessageCardView itemView = (TabGridMessageCardView) view;
        if (TabGridMessageCardViewProperties.ACTION_TEXT == propertyKey) {
            itemView.setAction(model.get(TabGridMessageCardViewProperties.ACTION_TEXT));
        } else if (TabGridMessageCardViewProperties.DESCRIPTION_TEXT == propertyKey) {
            itemView.setDescriptionText(
                    model.get(TabGridMessageCardViewProperties.DESCRIPTION_TEXT));
        } else if (TabGridMessageCardViewProperties.DESCRIPTION_TEXT_TEMPLATE == propertyKey) {
            itemView.setDescriptionTextTemplate(
                    model.get(TabGridMessageCardViewProperties.DESCRIPTION_TEXT_TEMPLATE));
        }
    }
}