// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_TEXT_WATCHER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;

import android.content.Context;
import android.support.annotation.Nullable;
import android.text.Editable;
import android.text.TextWatcher;
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
        mModel.set(FAKE_SEARCH_BOX_TEXT_WATCHER, new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable s) {
                if (s.length() == 0) return;
                mFakeSearchBoxDelegate.requestUrlFocus(s.toString());

                // This won't cause infinite loop since we checked s.length() == 0 above.
                s.clear();
            }
        });

        // Set the initial state.
        mModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, true);
        mModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE, false);

        // TODO(crbug.com/982018): Enable voice recognition button in the fake search box.
        // TODO(crbug.com/982018): Change the fake search box in dark mode.
    }
}
