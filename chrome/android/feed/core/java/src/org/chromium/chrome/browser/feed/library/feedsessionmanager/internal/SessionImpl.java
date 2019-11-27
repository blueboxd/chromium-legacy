// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelMutation;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import org.chromium.chrome.browser.feed.library.api.internal.store.SessionMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.Validators;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue.TaskType;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils.ElapsedTimeTracker;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;

import java.util.List;
import java.util.Set;

/** Implementation of a {@link Session}. */
// TODO: Tiktok doesn't allow HeadSessionImpl to extend SessionImpl
public class SessionImpl implements InitializableSession, Dumpable {
    private static final String TAG = "SessionImpl";

    protected final Store mStore;
    protected final TaskQueue mTaskQueue;
    protected final ThreadUtils mThreadUtils;
    protected final TimingUtils mTimingUtils;
    private final boolean mLimitPagingUpdates;
    private final SessionContentTracker mSessionContentTracker =
            new SessionContentTracker(/* supportsClearAll= */ false);

    // Allow creation of the session without a model provider, this becomes an unbound session
    /*@Nullable*/ protected ModelProvider mModelProvider;
    /*@Nullable*/ protected ViewDepthProvider mViewDepthProvider;
    protected boolean mLegacyHeadContent;

    protected String mSessionId;

    // operation counts for the dumper
    int mUpdateCount;

    SessionImpl(Store store, boolean limitPagingUpdates, TaskQueue taskQueue,
            TimingUtils timingUtils, ThreadUtils threadUtils) {
        this.mStore = store;
        this.mTaskQueue = taskQueue;
        this.mTimingUtils = timingUtils;
        this.mThreadUtils = threadUtils;
        this.mLimitPagingUpdates = limitPagingUpdates;
    }

    @Override
    public void bindModelProvider(
            /*@Nullable*/ ModelProvider modelProvider,
            /*@Nullable*/ ViewDepthProvider viewDepthProvider) {
        this.mModelProvider = modelProvider;
        this.mViewDepthProvider = viewDepthProvider;
    }

    @Override
    public void setSessionId(String sessionId) {
        this.mSessionId = sessionId;
    }

    @Override
    /*@Nullable*/
    public ModelProvider getModelProvider() {
        return mModelProvider;
    }

    @Override
    public void populateModelProvider(List<StreamStructure> head, boolean cachedBindings,
            boolean legacyHeadContent, UiContext uiContext) {
        Logger.i(TAG, "PopulateModelProvider %s items", head.size());
        this.mLegacyHeadContent = legacyHeadContent;
        mThreadUtils.checkNotMainThread();
        ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);

        if (mModelProvider != null) {
            ModelMutation modelMutation =
                    mModelProvider.edit()
                            .hasCachedBindings(cachedBindings)
                            .setSessionId(mSessionId)
                            .setMutationContext(
                                    new MutationContext.Builder().setUiContext(uiContext).build());

            // Walk through head and add all the DataOperations to the session.
            for (StreamStructure streamStructure : head) {
                String contentId = streamStructure.getContentId();
                switch (streamStructure.getOperation()) {
                    case UPDATE_OR_APPEND:
                        if (!mSessionContentTracker.contains(contentId)) {
                            modelMutation.addChild(streamStructure);
                        }
                        break;
                    case REMOVE:
                        modelMutation.removeChild(streamStructure);
                        break;
                    case REQUIRED_CONTENT:
                        // No-op, required content should not be passed into modelMutation.
                        break;
                    default:
                        Logger.e(TAG, "unsupported StreamDataOperation: %s",
                                streamStructure.getOperation());
                }
                mSessionContentTracker.update(streamStructure);
            }
            modelMutation.commit();
        } else {
            mSessionContentTracker.update(head);
        }

        timeTracker.stop("populateSession", mSessionId, "operations", head.size());
    }

    @Override
    public void updateSession(boolean clearHead, List<StreamStructure> streamStructures,
            int schemaVersion,
            /*@Nullable*/ MutationContext mutationContext) {
        String localSessionId = Validators.checkNotNull(mSessionId);
        if (clearHead) {
            if (shouldInvalidateModelProvider(mutationContext, localSessionId)) {
                if (mModelProvider != null) {
                    mModelProvider.invalidate();
                    Logger.i(TAG, "Invalidating Model Provider for session %s due to a clear head",
                            localSessionId);
                }
            } else {
                Logger.i(TAG, "Session %s not updated due to clearHead", localSessionId);
            }
            return;
        }
        mUpdateCount++;
        updateSessionInternal(streamStructures, mutationContext);
    }

    protected boolean shouldInvalidateModelProvider(
            /*@Nullable*/ MutationContext mutationContext, String sessionId) {
        if (mModelProvider != null && mutationContext != null
                && mutationContext.getContinuationToken() != null) {
            return sessionId.equals(mutationContext.getRequestingSessionId());
        }
        return false;
    }

    @Override
    public boolean invalidateOnResetHead() {
        return true;
    }

    void updateSessionInternal(
            List<StreamStructure> streamStructures, /*@Nullable*/ MutationContext mutationContext) {
        ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);
        StreamToken mutationSourceToken =
                mutationContext != null ? mutationContext.getContinuationToken() : null;
        if (mutationSourceToken != null) {
            String contentId = mutationSourceToken.getContentId();
            if (!mSessionContentTracker.contains(contentId)) {
                // Make sure that mutationSourceToken is a token in this session, if not, we don't
                // update the session.
                timeTracker.stop("updateSessionIgnored", mSessionId, "Token Not Found", contentId);
                Logger.i(TAG, "Token %s not found in session, ignoring update", contentId);
                return;
            } else if (mLimitPagingUpdates) {
                String mutationSessionId =
                        Validators.checkNotNull(mutationContext).getRequestingSessionId();
                if (mutationSessionId == null) {
                    Logger.i(TAG, "Request Session was not set, ignoring update");
                    return;
                }
                if (!mSessionId.equals(mutationSessionId)) {
                    Logger.i(TAG, "Limiting Updates, paging request made on another session");
                    return;
                }
            }
        }

        ModelMutation modelMutation = (mModelProvider != null) ? mModelProvider.edit() : null;
        if (modelMutation != null && mutationContext != null) {
            modelMutation.setMutationContext(mutationContext);
            if (mutationContext.getContinuationToken() != null) {
                modelMutation.hasCachedBindings(true);
            }
        }

        int updateCnt = 0;
        int addFeatureCnt = 0;
        int requiredContentCnt = 0;
        SessionMutation sessionMutation = mStore.editSession(mSessionId);
        for (StreamStructure streamStructure : streamStructures) {
            String contentId = streamStructure.getContentId();
            switch (streamStructure.getOperation()) {
                case UPDATE_OR_APPEND:
                    if (mSessionContentTracker.contains(contentId)) {
                        // TODO: This could leave an empty feature if contentKey already exists in
                        // the session with a different parent.
                        if (modelMutation != null) {
                            // this is an update operation so we can ignore it
                            modelMutation.updateChild(streamStructure);
                            updateCnt++;
                        }
                    } else {
                        sessionMutation.add(streamStructure);
                        if (modelMutation != null) {
                            modelMutation.addChild(streamStructure);
                        }
                        addFeatureCnt++;
                    }
                    break;
                case REMOVE:
                    sessionMutation.add(streamStructure);
                    if (modelMutation != null) {
                        modelMutation.removeChild(streamStructure);
                    }
                    break;
                case CLEAR_ALL:
                    Logger.i(TAG, "CLEAR_ALL not support on this session type");
                    break;
                case REQUIRED_CONTENT:
                    if (!mSessionContentTracker.contains(contentId)) {
                        sessionMutation.add(streamStructure);
                        requiredContentCnt++;
                    }
                    break;
                default:
                    Logger.e(
                            TAG, "Unknown operation, ignoring: %s", streamStructure.getOperation());
            }
            mSessionContentTracker.update(streamStructure);
        }

        // Commit the Model Provider mutation after the store is updated.
        int taskType = mutationContext != null && mutationContext.isUserInitiated()
                ? TaskType.IMMEDIATE
                : TaskType.USER_FACING;
        mTaskQueue.execute(Task.SESSION_MUTATION, taskType, sessionMutation::commit);
        if (modelMutation != null) {
            modelMutation.commit();
        }
        timeTracker.stop("updateSession", mSessionId, "features", addFeatureCnt, "updates",
                updateCnt, "requiredContent", requiredContentCnt);
    }

    @Override
    public String getSessionId() {
        return Validators.checkNotNull(mSessionId);
    }

    @Override
    public Set<String> getContentInSession() {
        return mSessionContentTracker.getContentIds();
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("sessionName").value(mSessionId);
        dumper.forKey("")
                .value((mModelProvider == null) ? "sessionIsUnbound" : "sessionIsBound")
                .compactPrevious();
        dumper.forKey("updateCount").value(mUpdateCount).compactPrevious();
        dumper.dump(mSessionContentTracker);
        if (mModelProvider instanceof Dumpable) {
            dumper.dump((Dumpable) mModelProvider);
        }
    }
}
