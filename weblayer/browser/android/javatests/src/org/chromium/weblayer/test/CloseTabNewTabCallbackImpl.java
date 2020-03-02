// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.weblayer.NewTabCallback;
import org.chromium.weblayer.Tab;

/**
 * NewTabCallback test helper. Primarily used to wait for a tab to be closed.
 */
public class CloseTabNewTabCallbackImpl extends NewTabCallback {
    private final CallbackHelper mCallbackHelper = new CallbackHelper();

    @Override
    public void onNewTab(Tab tab, int mode) {}

    @Override
    public void onCloseTab() {
        mCallbackHelper.notifyCalled();
    }

    public void waitForCloseTab() {
        try {
            // waitForFirst() only handles a single call. If you need more convert from
            // waitForFirst().
            mCallbackHelper.waitForFirst();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    public boolean hasClosed() {
        return mCallbackHelper.getCallCount() > 0;
    }
}
