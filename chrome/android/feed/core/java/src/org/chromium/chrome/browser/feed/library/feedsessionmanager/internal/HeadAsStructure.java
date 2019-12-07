// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import android.support.annotation.VisibleForTesting;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.Validators;
import org.chromium.chrome.browser.feed.library.common.functional.Function;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils.ElapsedTimeTracker;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.annotation.concurrent.GuardedBy;

/**
 * This class will load the current $HEAD to allow for structured access to the defined contents and
 * structure. Currently, the only supported access is filtering the tree in a pre-order traversal of
 * the structure, {@see #filter}.
 */
public final class HeadAsStructure {
    private static final String TAG = "HeadFilter";

    private final Store mStore;
    private final TimingUtils mTimingUtils;
    private final ThreadUtils mThreadUtils;
    private final Object mLock = new Object();
    @VisibleForTesting
    final Map<String, List<TreeNode>> mTree = new HashMap<>();
    @VisibleForTesting
    final Map<String, TreeNode> mContent = new HashMap<>();

    @GuardedBy("mLock")
    private boolean mInitalized;

    @VisibleForTesting
    TreeNode mRoot;

    /**
     * Define a Node within the tree formed by $HEAD. This contains both the structure and content.
     * Allowing filtering of either the structure or content.
     */
    public static final class TreeNode {
        final StreamStructure mStreamStructure;
        StreamPayload mStreamPayload;

        TreeNode(StreamStructure streamStructure) {
            this.mStreamStructure = streamStructure;
        }

        public StreamStructure getStreamStructure() {
            return mStreamStructure;
        }

        public StreamPayload getStreamPayload() {
            return mStreamPayload;
        }
    }

    public HeadAsStructure(Store store, TimingUtils timingUtils, ThreadUtils threadUtils) {
        this.mStore = store;
        this.mTimingUtils = timingUtils;
        this.mThreadUtils = threadUtils;
    }

    /**
     * Capture the state of the current $HEAD, returning success or failure through a {@link
     * Consumer}. The snapshot of head will not be updated if $HEAD is updated. Initialization may
     * only be called once. This method must be called on a background thread.
     */
    public void initialize(Consumer<Result<Void>> consumer) {
        Logger.i(TAG, "initialize HeadFilter");
        mThreadUtils.checkNotMainThread();
        ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);

        synchronized (mLock) {
            if (mInitalized) {
                consumer.accept(Result.failure());
                return;
            }
            if (!buildTree()) {
                timeTracker.stop("", "buildTree Failed");
                consumer.accept(Result.failure());
                return;
            }
            if (!bindChildren()) {
                timeTracker.stop("", "bindChildren Failed");
                consumer.accept(Result.failure());
                return;
            }
            mInitalized = true;
        }

        timeTracker.stop("task", "HeadFilter.initialize", "content", mContent.size());
        consumer.accept(Result.success(null));
    }

    /**
     * Using the current $HEAD, filter and transform the {@link TreeNode} stored at each node to
     * {@code T}. The {@code filterPredicate} will filter and transform the node. If {@code
     * filterPredicate} returns null, the value will be skipped.
     *
     * <p>This method must be called after {@link #mInitalized}. This method may run on the main
     * thread.
     */
    // The Nullness checker requires specifying the Nullable vs. NonNull state explicitly since it
    // can't be inferred from T.  This is done here and in the methods called below.
    public <T> Result<List</*@NonNull*/ T>> filter(
            Function<TreeNode, /*@Nullable*/ T> filterPredicate) {
        Logger.i(TAG, "filterHead");
        synchronized (mLock) {
            if (!mInitalized) {
                Logger.e(TAG, "HeadFilter has not been initialized");
                return Result.failure();
            }
        }

        ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);
        List</*@NonNull*/ T> filteredList = new ArrayList<>();
        traverseHead(filterPredicate, filteredList);
        Logger.i(TAG, "filterList size %s", filteredList.size());
        timeTracker.stop("task", "HeadFilter.filterHead");
        return Result.success(filteredList);
    }

    private <T> void traverseHead(
            Function<TreeNode, /*@Nullable*/ T> filterPredicate, List</*@NonNull*/ T> results) {
        TreeNode r = Validators.checkNotNull(mRoot);
        traverseNode(r, filterPredicate, results);
    }

    private <T> void traverseNode(TreeNode node,
            Function<TreeNode, /*@Nullable*/ T> filterPredicate, List</*@NonNull*/ T> results) {
        if (node.mStreamPayload == null) {
            Logger.w(TAG, "Found unbound node %s", node.mStreamStructure.getContentId());
            return;
        }
        T data = filterPredicate.apply(node);
        if (data != null) {
            results.add(data);
        }
        List<TreeNode> children = mTree.get(node.mStreamStructure.getContentId());
        if (children != null) {
            for (TreeNode child : children) {
                traverseNode(child, filterPredicate, results);
            }
        }
    }

    private boolean bindChildren() {
        Result<List<PayloadWithId>> payloadResult =
                mStore.getPayloads(new ArrayList<>(mContent.keySet()));
        if (!payloadResult.isSuccessful()) {
            Logger.e(TAG, "Unable to get payloads");
            return false;
        }
        for (PayloadWithId payloadWithId : payloadResult.getValue()) {
            TreeNode node = mContent.get(payloadWithId.contentId);
            if (node == null) {
                // This shouldn't happen
                Logger.w(TAG, "Unable to find tree content for %s", payloadWithId.contentId);
                continue;
            }
            node.mStreamPayload = payloadWithId.payload;
        }
        return true;
    }

    private boolean buildTree() {
        Result<List<StreamStructure>> headResult =
                mStore.getStreamStructures(Store.HEAD_SESSION_ID);
        if (!headResult.isSuccessful()) {
            Logger.e(TAG, "Unable to load $HEAD");
            return false;
        }
        List<StreamStructure> head = headResult.getValue();
        Logger.i(TAG, "size of $head %s", head.size());
        for (StreamStructure structure : head) {
            switch (structure.getOperation()) {
                case CLEAR_ALL:
                    continue;
                case UPDATE_OR_APPEND:
                    updateOrAppend(structure);
                    break;
                case REMOVE:
                    remove(structure);
                    break;
                default:
                    Logger.w(TAG, "Unsupported Operation %s", structure.getOperation());
                    break;
            }
        }
        if (mRoot == null) {
            Logger.e(TAG, "Root was not found");
            return false;
        }
        return true;
    }

    private void updateOrAppend(StreamStructure structure) {
        String contentId = structure.getContentId();
        if (mContent.containsKey(contentId)) {
            // this is an update, ignore it
            return;
        }
        TreeNode node = new TreeNode(structure);
        mContent.put(contentId, node);
        updateTreeStructure(contentId);
        if (!structure.hasParentContentId()) {
            // this is the root
            if (mRoot != null) {
                Logger.e(TAG, "Found Multiple roots");
            }
            mRoot = node;
            return;
        }

        // add this as a child of the parent
        List<TreeNode> parentChildren = updateTreeStructure(structure.getParentContentId());
        parentChildren.add(node);
    }

    private void remove(StreamStructure structure) {
        String contentId = structure.getContentId();
        String parentId = structure.hasParentContentId() ? structure.getParentContentId() : null;
        TreeNode node = mContent.get(contentId);
        if (node == null) {
            Logger.w(TAG, "Unable to find StreamStructure %s to remove", contentId);
            return;
        }
        if (parentId == null) {
            // Removing root is not supported, CLEAR_HEAD is intended when the root itself is
            // cleared.
            Logger.w(TAG, "Removing Root is not supported, unable to remove %s", contentId);
            return;
        }
        List<TreeNode> parentChildren = mTree.get(parentId);
        if (parentChildren == null) {
            Logger.w(TAG, "Parent %s not found, unable to remove", parentId, contentId);
        } else if (!parentChildren.remove(node)) {
            Logger.w(TAG, "Removing %s, not found in parent %s", contentId, parentId);
        }
        mTree.remove(contentId);
        mContent.remove(contentId);
    }

    private List<TreeNode> updateTreeStructure(String contentId) {
        List<TreeNode> children = mTree.get(contentId);
        if (children == null) {
            children = new ArrayList<>();
            mTree.put(contentId, children);
        }
        return children;
    }
}
