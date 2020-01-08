// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.internal;

import static org.chromium.chrome.browser.feed.library.feedstore.internal.FeedStoreConstants.SEMANTIC_PROPERTIES_PREFIX;
import static org.chromium.chrome.browser.feed.library.feedstore.internal.FeedStoreConstants.SHARED_STATE_PREFIX;
import static org.chromium.chrome.browser.feed.library.feedstore.internal.FeedStoreConstants.UPLOADABLE_ACTION_PREFIX;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorageDirect;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue.TaskType;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils.ElapsedTimeTracker;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamLocalAction;

import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

/** Storage Content Garbage Collector. */
public final class ContentGc {
    private static final String TAG = "ContentGc";

    private final Supplier<Set<String>> mAccessibleContentSupplier;
    private final Set<String> mReservedContentIds;
    private final Supplier<Set<StreamLocalAction>> mActionsSupplier;
    private final ContentStorageDirect mContentStorageDirect;
    private final TimingUtils mTimingUtils;
    private final TaskQueue mTaskQueue;
    private final boolean mKeepSharedStates;
    private final long mMaxAllowedGcAttempts;

    private int mContentGcAttempts;

    ContentGc(Configuration configuration, Supplier<Set<String>> accessibleContentSupplier,
            Set<String> reservedContentIds, Supplier<Set<StreamLocalAction>> actionsSupplier,
            ContentStorageDirect contentStorageDirect, TaskQueue taskQueue, TimingUtils timingUtils,
            boolean keepSharedStates) {
        this.mAccessibleContentSupplier = accessibleContentSupplier;
        this.mReservedContentIds = reservedContentIds;
        this.mActionsSupplier = actionsSupplier;
        this.mContentStorageDirect = contentStorageDirect;
        this.mTaskQueue = taskQueue;
        this.mTimingUtils = timingUtils;
        this.mKeepSharedStates = keepSharedStates;
        mMaxAllowedGcAttempts = configuration.getValueOrDefault(ConfigKey.MAXIMUM_GC_ATTEMPTS, 10L);
    }

    void gc() {
        if (mTaskQueue.hasBacklog() && mContentGcAttempts < mMaxAllowedGcAttempts) {
            Logger.i(TAG, "Re-enqueuing triggerContentGc; attempts(%d)", mContentGcAttempts);
            mContentGcAttempts++;
            mTaskQueue.execute(Task.GARBAGE_COLLECT_CONTENT, TaskType.BACKGROUND, this::gc);
            return;
        }

        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        Set<String> population = getPopulation();
        // remove the items in the population that are accessible, reserved, or semantic properties
        // either accessible or associated with an action
        Set<String> accessibleContent = getAccessible();
        population.removeAll(accessibleContent);
        population.removeAll(mReservedContentIds);
        population.removeAll(getAccessibleSemanticProperties(accessibleContent));
        population.removeAll(getLocalActionSemanticProperties(getLocalActions()));
        filterUploadableActions(population);
        if (mKeepSharedStates) {
            filterSharedStates(population);
        } else {
            population.removeAll(getAccessibleSharedStates(accessibleContent));
        }

        // Population now contains only un-accessible items
        removeUnAccessible(population);
        tracker.stop("task", "ContentGc", "contentItemsRemoved", population.size());
    }

    private void removeUnAccessible(Set<String> unAccessible) {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        ContentMutation.Builder mutationBuilder = new ContentMutation.Builder();
        for (String key : unAccessible) {
            Logger.i(TAG, "Removing %s", key);
            mutationBuilder.delete(key);
        }
        CommitResult result = mContentStorageDirect.commit(mutationBuilder.build());
        if (result == CommitResult.FAILURE) {
            Logger.e(TAG, "Content Modification failed removing unaccessible items.");
        }
        tracker.stop("", "removeUnAccessible", "mutations", unAccessible.size());
    }

    private void filterSharedStates(Set<String> population) {
        filterPrefix(population, SHARED_STATE_PREFIX);
    }

    private void filterUploadableActions(Set<String> population) {
        filterPrefix(population, UPLOADABLE_ACTION_PREFIX);
    }

    private void filterPrefix(Set<String> population, String prefix) {
        int size = population.size();
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        Iterator<String> i = population.iterator();
        while (i.hasNext()) {
            String key = i.next();
            if (key.startsWith(prefix)) {
                i.remove();
            }
        }
        tracker.stop("", "filterPrefix " + prefix, population.size() - size);
    }

    private Set<String> getAccessible() {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        Set<String> accessibleContent = mAccessibleContentSupplier.get();
        tracker.stop("", "getAccessible", "accessableContent", accessibleContent.size());
        return accessibleContent;
    }

    private Set<StreamLocalAction> getLocalActions() {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        Set<StreamLocalAction> actions = mActionsSupplier.get();
        tracker.stop("", "getLocalActions", "actionCount", actions.size());
        return actions;
    }

    private Set<String> getPopulation() {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        Set<String> population = new HashSet<>();
        Result<List<String>> result = mContentStorageDirect.getAllKeys();
        if (result.isSuccessful()) {
            population.addAll(result.getValue());
        } else {
            Logger.e(TAG, "Unable to get all content, getAll failed");
        }
        tracker.stop("", "getPopulation", "contentPopulation", population.size());
        return population;
    }

    private Set<String> getAccessibleSemanticProperties(Set<String> accessibleContent) {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        Set<String> semanticPropertiesKeys = new HashSet<>();
        for (String accessibleContentId : accessibleContent) {
            String semanticPropertyKey = SEMANTIC_PROPERTIES_PREFIX + accessibleContentId;
            semanticPropertiesKeys.add(semanticPropertyKey);
        }
        tracker.stop("", "getAccessibleSemanticProperties", "accessibleSemanticPropertiesSize",
                semanticPropertiesKeys.size());
        return semanticPropertiesKeys;
    }

    private Set<String> getAccessibleSharedStates(Set<String> accessibleContent) {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        Set<String> sharedStateKeys = new HashSet<>();
        for (String accessibleContentId : accessibleContent) {
            String sharedStateKey = SHARED_STATE_PREFIX + accessibleContentId;
            sharedStateKeys.add(sharedStateKey);
        }
        tracker.stop("", "getAccessibleSharedStates", "accessibleSharedStatesSize",
                sharedStateKeys.size());
        return sharedStateKeys;
    }

    private Set<String> getLocalActionSemanticProperties(Set<StreamLocalAction> actions) {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        Set<String> semanticPropertiesKeys = new HashSet<>();
        for (StreamLocalAction action : actions) {
            String semanticPropertyKey = SEMANTIC_PROPERTIES_PREFIX + action.getFeatureContentId();
            semanticPropertiesKeys.add(semanticPropertyKey);
        }
        tracker.stop("", "getLocalActionSemanticProperties", "actionSemanticPropertiesSize",
                semanticPropertiesKeys.size());
        return semanticPropertiesKeys;
    }
}
