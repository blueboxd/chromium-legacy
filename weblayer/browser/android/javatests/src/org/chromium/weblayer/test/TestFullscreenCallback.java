// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.weblayer.FullscreenCallback;

/**
 * FullscreenCallback implementation for tests.
 */
public class TestFullscreenCallback extends FullscreenCallback {
    public int mEnterFullscreenCount;
    public int mExitFullscreenCount;
    public Runnable mExitFullscreenRunnable;

    @Override
    public void onEnterFullscreen(Runnable exitFullscreenRunner) {
        mEnterFullscreenCount++;
        mExitFullscreenRunnable = exitFullscreenRunner;
    }

    @Override
    public void onExitFullscreen() {
        mExitFullscreenCount++;
    }

    public void waitForFullscreen() {
        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> mEnterFullscreenCount));
    }

    public void waitForExitFullscreen() {
        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> mExitFullscreenCount));
    }
}
