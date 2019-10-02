// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL;

import android.content.Context;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for handling {@link TasksSurface}-related logic.
 */
class TasksSurfaceMediator {
    private final PropertyModel mModel;

    TasksSurfaceMediator(Context context, PropertyModel model, boolean isTabCarousel) {
        mModel = model;
        mModel.set(IS_TAB_CAROUSEL, isTabCarousel);
    }
}
