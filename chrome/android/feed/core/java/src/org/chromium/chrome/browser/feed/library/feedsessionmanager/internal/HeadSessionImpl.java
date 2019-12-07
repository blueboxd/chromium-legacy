// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.store.SessionMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils.ElapsedTimeTracker;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;

import java.util.List;
import java.util.Set;

/**
 * Implementation of {@link Session} for $HEAD. This class doesn't support a ModelProvider. The
 * $HEAD session does not support optimistic writes because we may create a new session between when
 * the response is received and when task updating the head session runs.
 */
public class HeadSessionImpl implements Session, Dumpable {
    private static final String TAG = "HeadSessionImpl";

    private final Store mStore;
    private final TimingUtils mTimingUtils;
    private final SessionContentTracker mSessionContentTracker =
            new SessionContentTracker(/* supportsClearAll= */ true);
    private final boolean mLimitPageUpdatesInHead;

    private int mSchemaVersion;

    // operation counts for the dumper
    private int mUpdateCount;
    private int mStoreMutationFailures;

    HeadSessionImpl(Store store, TimingUtils timingUtils, boolean limitPageUpdatesInHead) {
        this.mStore = store;
        this.mTimingUtils = timingUtils;
        this.mLimitPageUpdatesInHead = limitPageUpdatesInHead;
    }

    /** Initialize head from the stored stream structures. */
    void initializeSession(List<StreamStructure> streamStructures, int schemaVersion) {
        Logger.i(TAG, "Initialize HEAD %s items", streamStructures.size());
        this.mSchemaVersion = schemaVersion;
        mSessionContentTracker.update(streamStructures);
    }

    void reset() {
        mSessionContentTracker.clear();
    }

    public int getSchemaVersion() {
        return mSchemaVersion;
    }

    @Override
    public boolean invalidateOnResetHead() {
        // There won't be a ModelProvider to invalidate
        return false;
    }

    @Override
    public void updateSession(boolean clearHead, List<StreamStructure> streamStructures,
            int schemaVersion, @Nullable MutationContext mutationContext) {
        ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);
        mUpdateCount++;

        if (clearHead) {
            this.mSchemaVersion = schemaVersion;
        }

        StreamToken mutationSourceToken =
                mutationContext != null ? mutationContext.getContinuationToken() : null;
        if (mutationSourceToken != null) {
            String contentId = mutationSourceToken.getContentId();
            if (mutationSourceToken.hasContentId() && !mSessionContentTracker.contains(contentId)) {
                // Make sure that mutationSourceToken is a token in this session, if not, we don't
                // update the session.
                timeTracker.stop(
                        "updateSessionIgnored", getSessionId(), "Token Not Found", contentId);
                Logger.i(TAG, "Token %s not found in session, ignoring update", contentId);
                return;
            } else if (mLimitPageUpdatesInHead) {
                timeTracker.stop("updateSessionIgnored", getSessionId());
                Logger.i(TAG, "Limited paging updates in HEAD");
                return;
            }
        }

        int updateCnt = 0;
        int addFeatureCnt = 0;
        int requiredContentCnt = 0;
        boolean cleared = false;
        SessionMutation sessionMutation = mStore.editSession(getSessionId());
        for (StreamStructure streamStructure : streamStructures) {
            String contentId = streamStructure.getContentId();
            switch (streamStructure.getOperation()) {
                case UPDATE_OR_APPEND:
                    if (mSessionContentTracker.contains(contentId)) {
                        // this is an update operation so we can ignore it
                        updateCnt++;
                    } else {
                        sessionMutation.add(streamStructure);
                        addFeatureCnt++;
                    }
                    break;
                case REMOVE:
                    Logger.i(TAG, "Removing Item %s from $HEAD", contentId);
                    if (mSessionContentTracker.contains(contentId)) {
                        sessionMutation.add(streamStructure);
                    } else {
                        Logger.w(TAG, "Remove operation content not found in $HEAD");
                    }
                    break;
                case CLEAR_ALL:
                    cleared = true;
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
        boolean success = sessionMutation.commit();
        if (success) {
            timeTracker.stop("updateSession", getSessionId(), "cleared", cleared, "features",
                    addFeatureCnt, "updates", updateCnt, "requiredContent", requiredContentCnt);
        } else {
            timeTracker.stop("updateSessionFailure", getSessionId());
            mStoreMutationFailures++;
            Logger.e(TAG, "$HEAD session mutation failed");
            mStore.switchToEphemeralMode();
        }
    }

    @Override
    public String getSessionId() {
        return Store.HEAD_SESSION_ID;
    }

    @Override
    @Nullable
    public ModelProvider getModelProvider() {
        return null;
    }

    @Override
    public Set<String> getContentInSession() {
        return mSessionContentTracker.getContentIds();
    }

    public boolean isHeadEmpty() {
        return mSessionContentTracker.isEmpty();
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("sessionName").value(getSessionId());
        dumper.forKey("updateCount").value(mUpdateCount).compactPrevious();
        dumper.forKey("storeMutationFailures").value(mStoreMutationFailures).compactPrevious();
        dumper.dump(mSessionContentTracker);
    }
}
