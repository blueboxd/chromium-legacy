// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.BottomBarProperties.IS_INCOGNITO;
import static org.chromium.chrome.features.start_surface.BottomBarProperties.IS_VISIBLE;
import static org.chromium.chrome.features.start_surface.BottomBarProperties.ON_CLICK_LISTENER;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The view binder of the bottom bar. */
class BottomBarViewBinder {
    public static void bind(PropertyModel model, BottomBarView view, PropertyKey propertyKey) {
        if (IS_INCOGNITO == propertyKey) {
            view.setIncognito(model.get(IS_INCOGNITO));
        } else if (IS_VISIBLE == propertyKey) {
            view.setVisibility(model.get(IS_VISIBLE));
        } else if (ON_CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(ON_CLICK_LISTENER));
        }
    }
}