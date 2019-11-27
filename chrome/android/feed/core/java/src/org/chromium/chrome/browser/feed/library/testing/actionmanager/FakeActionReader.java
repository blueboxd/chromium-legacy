// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.actionmanager;

import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionReader;
import org.chromium.chrome.browser.feed.library.api.internal.common.DismissActionWithSemanticProperties;
import org.chromium.chrome.browser.feed.library.common.Result;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Fake implementation of {@link ActionReader}. */
public final class FakeActionReader implements ActionReader {
    private final ArrayList<DismissActionWithSemanticProperties> mDismissActions =
            new ArrayList<>();

    @Override
    public Result<List<DismissActionWithSemanticProperties>>
    getDismissActionsWithSemanticProperties() {
        return Result.success(mDismissActions);
    }

    /** Adds a dismiss action with semantic properties. */
    public FakeActionReader addDismissActionsWithSemanticProperties(
            DismissActionWithSemanticProperties... dismissActionsToAdd) {
        Collections.addAll(mDismissActions, dismissActionsToAdd);
        return this;
    }
}
