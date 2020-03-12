// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild.Type;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.Validators;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** SessionImpl which implements the TimeoutScheduler specific behaviors need within the session. */
public final class TimeoutSessionImpl extends SessionImpl {
    private static final String TAG = "TimeoutSessionImpl";

    TimeoutSessionImpl(Store store, boolean limitPagingUpdates, TaskQueue taskQueue,
            TimingUtils timingUtils, ThreadUtils threadUtils) {
        super(store, limitPagingUpdates, taskQueue, timingUtils, threadUtils);
        Logger.i(TAG, "Using TimeoutSessionImpl");
    }

    @Override
    public void populateModelProvider(List<StreamStructure> head, boolean cachedBindings,
            boolean legacyHeadContent, UiContext uiContext) {
        Logger.i(TAG, "TimeoutSession.populateModelProvider, shouldAppend %s", legacyHeadContent);
        super.populateModelProvider(head, cachedBindings, legacyHeadContent, uiContext);
    }

    @Override
    public void updateSession(boolean clearHead, List<StreamStructure> streamStructures,
            int schemaVersion, @Nullable MutationContext mutationContext) {
        String localSessionId = Validators.checkNotNull(mSessionId);
        Logger.i(TAG, "updateSession; clearHead(%b), shouldAppend(%b), sessionId(%s)", clearHead,
                mLegacyHeadContent, localSessionId);
        if (clearHead) {
            if (!mLegacyHeadContent) {
                if (shouldInvalidateModelProvider(mutationContext, localSessionId)) {
                    if (mModelProvider != null) {
                        if (mutationContext == null) {
                            mModelProvider.invalidate(UiContext.getDefaultInstance());
                        } else {
                            mModelProvider.invalidate(mutationContext.getUiContext());
                        }
                        Logger.i(TAG,
                                "Invalidating Model Provider for session %s due to a clear head",
                                localSessionId);
                    }
                } else {
                    // Without having legacy HEAD content, don't update the session,
                    Logger.i(TAG, "Not configured to append: %s", localSessionId);
                }
                return;
            }

            if (mViewDepthProvider != null) {
                // Append the new items to the existing copy of HEAD, removing the existing items
                // which have not yet been seen by the user.
                List<ModelChild> rootChildren = captureRootContent();
                if (!rootChildren.isEmpty()) {
                    // Calculate the children to remove and append StreamStructure remove operations
                    String lowestChild =
                            Validators.checkNotNull(mViewDepthProvider).getChildViewDepth();
                    List<StreamStructure> removeOperations = removeItems(lowestChild, rootChildren);
                    Logger.i(TAG, "Removing %d items", removeOperations.size());
                    if (!removeOperations.isEmpty()) {
                        removeOperations.addAll(streamStructures);
                        streamStructures = removeOperations;
                    }
                }
            }
            // Only do this once
            mLegacyHeadContent = false;
        }

        mUpdateCount++;
        updateSessionInternal(streamStructures, mutationContext);
    }

    @Override
    public boolean invalidateOnResetHead() {
        return false;
    }

    /**
     * Remove items found below the specified {@code lowestChild}. If {@code lowestChild} is itself
     * a token, it will also be removed to avoid multiple "More" buttons. If {@code lowestChild} is
     * {@literal null}, only tokens will be removed to prevent potentially removing items that the
     * user can currently see.
     */
    private List<StreamStructure> removeItems(
            @Nullable String lowestChild, List<ModelChild> rootChildren) {
        boolean remove = false;
        List<StreamStructure> removeOperations = new ArrayList<>();
        for (ModelChild child : rootChildren) {
            if (remove || child.getType() == Type.TOKEN) {
                removeOperations.add(
                        createRemoveFeature(child.getContentId(), child.getParentId()));
            } else if (child.getContentId().equals(lowestChild)) {
                remove = true;
            }
        }
        return removeOperations;
    }

    @VisibleForTesting
    List<ModelChild> captureRootContent() {
        ModelProvider modelProvider = getModelProvider();
        if (modelProvider == null) {
            Logger.w(TAG, "ModelProvider was not found");
            return Collections.emptyList();
        }
        return modelProvider.getAllRootChildren();
    }

    @VisibleForTesting
    StreamStructure createRemoveFeature(String contentId, @Nullable String parentId) {
        StreamStructure.Builder builder =
                StreamStructure.newBuilder().setOperation(Operation.REMOVE).setContentId(contentId);
        if (parentId != null) {
            builder.setParentContentId(parentId);
        }
        return builder.build();
    }
}
