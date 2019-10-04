// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.View;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for handling {@link TasksSurface}-related logic.
 */
class TasksSurfaceMediator {
    @Nullable
    private final TasksSurface.FakeSearchBoxDelegate mFakeSearchBoxDelegate;
    private final PropertyModel mModel;

    TasksSurfaceMediator(Context context, PropertyModel model,
            TasksSurface.FakeSearchBoxDelegate fakeSearchBoxDelegate, boolean isTabCarousel) {
        mFakeSearchBoxDelegate = fakeSearchBoxDelegate;
        assert mFakeSearchBoxDelegate != null;

        mModel = model;
        mModel.set(IS_TAB_CAROUSEL, isTabCarousel);
        mModel.set(FAKE_SEARCH_BOX_CLICK_LISTENER, new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mFakeSearchBoxDelegate.requestUrlFocus(null);
            }
        });

        // Set the initial state.
        mModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, true);
        mModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE, false);

        // TODO(crbug.com/982018): Enable voice recognition button in the fake search box.
        // TODO(crbug.com/982018): Enable long press and paste search in the fake search box.
        // TODO(crbug.com/982018): Change the fake search box in dark mode.
    }
}
