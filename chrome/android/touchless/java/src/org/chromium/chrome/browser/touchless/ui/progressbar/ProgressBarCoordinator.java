// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.progressbar;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * A class to manage the touchless progress bar UI element.
 */
public class ProgressBarCoordinator {
    private final ProgressBarMediator mMediator;

    /**
     * @param activityTabProvider Progress will be displayed for {@link Tab}s that are added to
     * this {@link ActivityTabProvider}.
     * @param progressBarView {@link ProgressBarView} used for displaying current progress and
     * eTLD+1.
     */
    public ProgressBarCoordinator(
            ProgressBarView progressBarView, ActivityTabProvider activityTabProvider) {
        PropertyModel model = new PropertyModel.Builder(ProgressBarProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(model, progressBarView, ProgressBarViewBinder::bind);
        mMediator = new ProgressBarMediator(model, activityTabProvider);
    }

    public void onActivityResume() {
        mMediator.onActivityResume();
    }

    public void destroy() {
        mMediator.destroy();
    }
}
