// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.actionmanager;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ScrollListener;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ViewActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.api.internal.store.UploadableActionMutation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;
import org.chromium.components.feed.core.proto.wire.ActionPayloadProto.ActionPayload;

import java.util.HashSet;
import java.util.Set;

/** Fake implementation of {@link ViewActionManager}. */
public final class FakeViewActionManager implements ViewActionManager {
    private final Store mStore;
    public final Set<StreamUploadableAction> mViewActions = new HashSet<>();

    public FakeViewActionManager(Store store) {
        mStore = store;
    }

    @Override
    public void setViewport(@Nullable View viewport) {}

    @Override
    public void onViewVisible(View view, String contentId, ActionPayload actionPayload) {}

    @Override
    public void onViewHidden(View view, String contentId) {}

    @Override
    public ScrollListener getScrollListener() {
        return new ScrollListener() {
            @Override
            public void onScrollStateChanged(int state) {}

            @Override
            public void onScrolled(int dx, int dy) {}
        };
    }

    @Override
    public void onAnimationFinished() {}

    @Override
    public void onLayoutChange() {}

    @Override
    public void onShow() {}

    @Override
    public void onHide() {}

    @Override
    public void storeViewActions(Runnable runnable) {
        UploadableActionMutation actionMutation = mStore.editUploadableActions();
        for (StreamUploadableAction action : mViewActions) {
            actionMutation.upsert(action, action.getFeatureContentId());
        }
        CommitResult commitResult = actionMutation.commit();
        runnable.run();
    }
}
