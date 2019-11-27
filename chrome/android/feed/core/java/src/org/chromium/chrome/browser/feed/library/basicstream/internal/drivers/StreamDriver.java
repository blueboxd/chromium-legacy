// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkNotNull;

import android.content.Context;
import android.support.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.logging.ZeroStateShowReason;
import org.chromium.chrome.browser.feed.library.api.host.stream.SnackbarApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.SnackbarCallbackApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipApi;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParserFactory;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.knowncontent.FeedKnownContent;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChange;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChangeObserver;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild.Type;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelCursor;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.State;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.ContinuationDriver.CursorChangedListener;
import org.chromium.chrome.browser.feed.library.basicstream.internal.pendingdismiss.PendingDismissHandler;
import org.chromium.chrome.browser.feed.library.basicstream.internal.scroll.BasicStreamScrollMonitor;
import org.chromium.chrome.browser.feed.library.basicstream.internal.scroll.BasicStreamScrollTracker;
import org.chromium.chrome.browser.feed.library.basicstream.internal.scroll.ScrollRestorer;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager.ContextMenuManager;
import org.chromium.chrome.browser.feed.library.sharedstream.offlinemonitor.StreamOfflineMonitor;
import org.chromium.chrome.browser.feed.library.sharedstream.pendingdismiss.PendingDismissCallback;
import org.chromium.chrome.browser.feed.library.sharedstream.removetrackingfactory.StreamRemoveTrackingFactory;
import org.chromium.chrome.browser.feed.library.sharedstream.scroll.ScrollLogger;
import org.chromium.chrome.browser.feed.library.sharedstream.scroll.ScrollTracker;
import org.chromium.components.feed.core.proto.libraries.sharedstream.UiRefreshReasonProto.UiRefreshReason;
import org.chromium.components.feed.core.proto.libraries.sharedstream.UiRefreshReasonProto.UiRefreshReason.Reason;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.UndoAction;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/** Generates a list of {@link LeafFeatureDriver} instances for an entire stream. */
public class StreamDriver
        implements CursorChangedListener, FeatureChangeObserver, PendingDismissHandler {
    private static final String TAG = "StreamDriver";
    private final ActionApi mActionApi;
    private final ActionManager mActionManager;
    private final ActionParserFactory mActionParserFactory;
    private final ThreadUtils mThreadUtils;
    private final ModelProvider mModelProvider;
    private final Map<ModelChild, FeatureDriver> mModelChildFeatureDriverMap;
    private final List<FeatureDriver> mFeatureDrivers;
    private final Clock mClock;
    private final Configuration mConfiguration;
    private final Context mContext;
    private final SnackbarApi mSnackbarApi;
    private final ContentChangedListener mContentChangedListener;
    private final ScrollRestorer mScrollRestorer;
    private final BasicLoggingApi mBasicLoggingApi;
    private final StreamOfflineMonitor mStreamOfflineMonitor;
    private final ContextMenuManager mContextMenuManager;
    private final boolean mIsInitialLoad;
    private final MainThreadRunner mMainThreadRunner;
    private final ViewLoggingUpdater mViewLoggingUpdater;
    private final TooltipApi mTooltipApi;
    private final UiRefreshReason mUiRefreshReason;
    private final ScrollTracker mScrollTracker;

    private boolean mRestoring;
    private boolean mRootFeatureConsumed;
    private boolean mModelFeatureChangeObserverRegistered;
    /*@Nullable*/ private StreamContentListener mContentListener;

    public StreamDriver(ActionApi actionApi, ActionManager actionManager,
            ActionParserFactory actionParserFactory, ModelProvider modelProvider,
            ThreadUtils threadUtils, Clock clock, Configuration configuration, Context context,
            SnackbarApi snackbarApi, ContentChangedListener contentChangedListener,
            ScrollRestorer scrollRestorer, BasicLoggingApi basicLoggingApi,
            StreamOfflineMonitor streamOfflineMonitor, FeedKnownContent feedKnownContent,
            ContextMenuManager contextMenuManager, boolean restoring, boolean isInitialLoad,
            MainThreadRunner mainThreadRunner, ViewLoggingUpdater viewLoggingUpdater,
            TooltipApi tooltipApi, UiRefreshReason uiRefreshReason,
            BasicStreamScrollMonitor scrollMonitor) {
        this.mActionApi = actionApi;
        this.mActionManager = actionManager;
        this.mActionParserFactory = actionParserFactory;
        this.mThreadUtils = threadUtils;
        this.mModelProvider = modelProvider;
        this.mClock = clock;
        this.mContext = context;
        this.mSnackbarApi = snackbarApi;
        this.mContextMenuManager = contextMenuManager;
        this.mModelChildFeatureDriverMap = new HashMap<>();
        this.mFeatureDrivers = new ArrayList<>();
        this.mConfiguration = configuration;
        this.mContentChangedListener = contentChangedListener;
        this.mScrollRestorer = scrollRestorer;
        this.mBasicLoggingApi = basicLoggingApi;
        this.mStreamOfflineMonitor = streamOfflineMonitor;
        this.mRestoring = restoring;
        this.mIsInitialLoad = isInitialLoad;
        this.mMainThreadRunner = mainThreadRunner;
        this.mViewLoggingUpdater = viewLoggingUpdater;
        this.mTooltipApi = tooltipApi;
        this.mUiRefreshReason = uiRefreshReason;
        mScrollTracker = new BasicStreamScrollTracker(
                mainThreadRunner, new ScrollLogger(basicLoggingApi), clock, scrollMonitor);

        modelProvider.enableRemoveTracking(
                new StreamRemoveTrackingFactory(modelProvider, feedKnownContent));
    }

    /**
     * Returns a the list of {@link LeafFeatureDriver} instances for the children generated from the
     * given {@link ModelFeature}.
     */
    public List<LeafFeatureDriver> getLeafFeatureDrivers() {
        if (mModelProvider.getCurrentState() == State.READY && !mRootFeatureConsumed) {
            ChildCreationPayload childCreationPayload;
            mRootFeatureConsumed = true;
            ModelFeature rootFeature = mModelProvider.getRootFeature();
            if (rootFeature != null) {
                childCreationPayload = createAndInsertChildren(rootFeature, mModelProvider);
                rootFeature.registerObserver(this);
                mModelFeatureChangeObserverRegistered = true;
                if (mUiRefreshReason.getReason().equals(Reason.ZERO_STATE)) {
                    mBasicLoggingApi.onZeroStateRefreshCompleted(
                            childCreationPayload.mContentCount, childCreationPayload.mTokenCount);
                }
            } else {
                mBasicLoggingApi.onInternalError(InternalFeedError.NO_ROOT_FEATURE);
                Logger.w(TAG, "found null root feature loading Leaf Feature Drivers");
                if (mUiRefreshReason.getReason().equals(Reason.ZERO_STATE)) {
                    mBasicLoggingApi.onZeroStateRefreshCompleted(
                            /* newContentCount= */ 0, /* newTokenCount= */ 0);
                }
            }
        }

        if (!mIsInitialLoad) {
            addNoContentCardOrZeroStateIfNecessary(ZeroStateShowReason.NO_CONTENT);
        }

        return buildLeafFeatureDrivers(mFeatureDrivers);
    }

    public void maybeRestoreScroll() {
        if (!mRestoring) {
            return;
        }

        if (isLastDriverContinuationDriver()) {
            ContinuationDriver continuationDriver =
                    (ContinuationDriver) mFeatureDrivers.get(mFeatureDrivers.size() - 1);
            // If there is a synthetic token, we should only restore if it has not been handled yet.
            if (continuationDriver.hasTokenBeenHandled()) {
                return;
            }
        }

        mRestoring = false;

        mScrollRestorer.maybeRestoreScroll();
    }

    private ChildCreationPayload createAndInsertChildren(
            ModelFeature streamFeature, ModelProvider modelProvider) {
        return createAndInsertChildren(streamFeature.getCursor(), modelProvider);
    }

    private ChildCreationPayload createAndInsertChildren(
            ModelCursor streamCursor, ModelProvider modelProvider) {
        return createAndInsertChildrenAtIndex(streamCursor, modelProvider, 0);
    }

    private ChildCreationPayload createAndInsertChildrenAtIndex(
            ModelCursor streamCursor, ModelProvider modelProvider, int insertionIndex) {
        Iterable<ModelChild> cursorIterable = () -> new Iterator<ModelChild>() {
            /*@Nullable*/ private ModelChild mNext;

            @Override
            public boolean hasNext() {
                mNext = streamCursor.getNextItem();
                return mNext != null;
            }

            @Override
            public ModelChild next() {
                return checkNotNull(mNext);
            }
        };
        return createAndInsertChildrenAtIndex(cursorIterable, modelProvider, insertionIndex);
    }

    private ChildCreationPayload createAndInsertChildrenAtIndex(
            Iterable<ModelChild> modelChildren, ModelProvider modelProvider, int insertionIndex) {
        List<FeatureDriver> newChildren = new ArrayList<>();

        int tokenCount = 0;
        int contentCount = 0;

        for (ModelChild child : modelChildren) {
            FeatureDriver featureDriverChild = createChild(child, modelProvider, insertionIndex);
            if (featureDriverChild != null) {
                if (child.getType() == Type.FEATURE) {
                    contentCount++;
                } else if (child.getType() == Type.TOKEN) {
                    tokenCount++;
                }
                newChildren.add(featureDriverChild);
                mFeatureDrivers.add(insertionIndex, featureDriverChild);
                mModelChildFeatureDriverMap.put(child, featureDriverChild);
                insertionIndex++;
            }
        }

        return new ChildCreationPayload(newChildren, tokenCount, contentCount);
    }

    /*@Nullable*/
    private FeatureDriver createChild(ModelChild child, ModelProvider modelProvider, int position) {
        switch (child.getType()) {
            case Type.FEATURE:
                return createFeatureChild(child.getModelFeature(), position);
            case Type.TOKEN:
                ContinuationDriver continuationDriver =
                        createContinuationDriver(mBasicLoggingApi, mClock, mConfiguration, mContext,
                                child, modelProvider, position, mSnackbarApi, mRestoring);

                // TODO: Look into moving initialize() into a more generic location. We don't
                // really want work to be done in the constructor so we call an initialize() method
                // to kick off any expensive work the driver may need to do.
                continuationDriver.initialize();
                return continuationDriver;
            case Type.UNBOUND:
                mBasicLoggingApi.onInternalError(InternalFeedError.TOP_LEVEL_UNBOUND_CHILD);
                Logger.e(TAG, "Found unbound child %s, ignoring it", child.getContentId());
                return null;
            default:
                Logger.wtf(TAG, "Received illegal child: %s from cursor.", child.getType());
                return null;
        }
    }

    private /*@Nullable*/ FeatureDriver createFeatureChild(
            ModelFeature modelFeature, int position) {
        if (modelFeature.getStreamFeature().hasCard()) {
            return createCardDriver(modelFeature, position);
        } else if (modelFeature.getStreamFeature().hasCluster()) {
            return createClusterDriver(modelFeature, position);
        }

        mBasicLoggingApi.onInternalError(InternalFeedError.TOP_LEVEL_INVALID_FEATURE_TYPE);
        Logger.w(TAG, "Invalid StreamFeature Type, must be Card or Cluster but was %s",
                modelFeature.getStreamFeature().getFeaturePayloadCase());
        return null;
    }

    private List<LeafFeatureDriver> buildLeafFeatureDrivers(List<FeatureDriver> featureDrivers) {
        List<LeafFeatureDriver> leafFeatureDrivers = new ArrayList<>();
        List<FeatureDriver> removes = new ArrayList<>();
        for (FeatureDriver featureDriver : featureDrivers) {
            LeafFeatureDriver leafFeatureDriver = featureDriver.getLeafFeatureDriver();
            if (leafFeatureDriver != null) {
                leafFeatureDrivers.add(leafFeatureDriver);
            } else {
                mBasicLoggingApi.onInternalError(InternalFeedError.FAILED_TO_CREATE_LEAF);
                removes.add(featureDriver);
            }
        }
        for (FeatureDriver driver : removes) {
            this.mFeatureDrivers.remove(driver);
            driver.onDestroy();
        }

        mStreamOfflineMonitor.requestOfflineStatusForNewContent();

        return leafFeatureDrivers;
    }

    @Override
    public void onChange(FeatureChange change) {
        Logger.v(TAG, "Received change.");

        List<ModelChild> removedChildren = change.getChildChanges().getRemovedChildren();

        for (ModelChild removedChild : removedChildren) {
            if (!(removedChild.getType() == Type.FEATURE || removedChild.getType() == Type.TOKEN)) {
                Logger.e(TAG, "Attempting to remove non-removable child of type: %s",
                        removedChild.getType());
                continue;
            }

            removeDriver(removedChild);
        }

        List<ModelChild> appendedChildren = change.getChildChanges().getAppendedChildren();

        if (!appendedChildren.isEmpty()) {
            int insertionIndex = mFeatureDrivers.size();

            notifyContentsAdded(insertionIndex,
                    buildLeafFeatureDrivers(createAndInsertChildrenAtIndex(
                            appendedChildren, mModelProvider, insertionIndex)
                                                    .mFeatureDrivers));
        }

        addNoContentCardOrZeroStateIfNecessary(ZeroStateShowReason.CONTENT_DISMISSED);
    }

    @Override
    public void onNewChildren(
            ModelChild modelChild, List<ModelChild> modelChildren, boolean wasSynthetic) {
        int continuationIndex = removeDriver(modelChild);
        if (continuationIndex < 0) {
            Logger.wtf(TAG, "Received an onNewChildren for an unknown child.");
            return;
        }
        ChildCreationPayload tokenPayload =
                createAndInsertChildrenAtIndex(modelChildren, mModelProvider, continuationIndex);

        List<FeatureDriver> newChildren = tokenPayload.mFeatureDrivers;

        mBasicLoggingApi.onTokenCompleted(
                wasSynthetic, tokenPayload.mContentCount, tokenPayload.mTokenCount);
        notifyContentsAdded(continuationIndex, buildLeafFeatureDrivers(newChildren));
        maybeRemoveNoContentOrZeroStateCard();
        // Swap no content card with zero state if there are no more drivers.
        if (newChildren.isEmpty() && mFeatureDrivers.size() == 1
                && mFeatureDrivers.get(0) instanceof NoContentDriver) {
            showZeroState(ZeroStateShowReason.NO_CONTENT_FROM_CONTINUATION_TOKEN);
        }

        maybeRestoreScroll();
    }

    private void alwaysRemoveNoContentOrZeroStateCardIfPresent() {
        if (mFeatureDrivers.size() == 1
                && ((mFeatureDrivers.get(0) instanceof NoContentDriver)
                        || (mFeatureDrivers.get(0) instanceof ZeroStateDriver))) {
            mFeatureDrivers.get(0).onDestroy();
            mFeatureDrivers.remove(0);
            notifyContentRemoved(0);
        }
    }

    private void maybeRemoveNoContentOrZeroStateCard() {
        if (shouldRemoveNoContentCardOrZeroState()) {
            mFeatureDrivers.get(0).onDestroy();
            mFeatureDrivers.remove(0);
            notifyContentRemoved(0);
        }
    }

    public void onDestroy() {
        for (FeatureDriver featureDriver : mFeatureDrivers) {
            featureDriver.onDestroy();
        }
        ModelFeature modelFeature = mModelProvider.getRootFeature();
        if (modelFeature != null && mModelFeatureChangeObserverRegistered) {
            modelFeature.unregisterObserver(this);
            mModelFeatureChangeObserverRegistered = false;
        }
        mFeatureDrivers.clear();
        mModelChildFeatureDriverMap.clear();
        mScrollTracker.onUnbind();
    }

    public boolean hasContent() {
        if (mFeatureDrivers.isEmpty()) {
            return false;
        }
        return !(mFeatureDrivers.get(0) instanceof NoContentDriver)
                && !(mFeatureDrivers.get(0) instanceof ZeroStateDriver);
    }

    public boolean isZeroStateBeingShown() {
        return !mFeatureDrivers.isEmpty() && mFeatureDrivers.get(0) instanceof ZeroStateDriver
                && !((ZeroStateDriver) mFeatureDrivers.get(0)).isSpinnerShowing();
    }

    public void setModelProviderForZeroState(ModelProvider modelProvider) {
        if (!isZeroStateBeingShown()) {
            Logger.wtf(TAG,
                    "setModelProviderForZeroState should only be called when zero state is shown");
            return;
        }
        ((ZeroStateDriver) mFeatureDrivers.get(0)).setModelProvider(modelProvider);
    }

    private void addNoContentCardOrZeroStateIfNecessary(
            @ZeroStateShowReason int zeroStateShowReason) {
        LeafFeatureDriver leafFeatureDriver = null;
        if (!mRestoring && mFeatureDrivers.isEmpty()) {
            leafFeatureDriver = createAndLogZeroState(zeroStateShowReason);
        } else if (mFeatureDrivers.size() == 1 && isLastDriverContinuationDriver()) {
            leafFeatureDriver = createNoContentDriver();
        }

        if (leafFeatureDriver != null) {
            mFeatureDrivers.add(0, leafFeatureDriver);
            notifyContentsAdded(0, Collections.singletonList(leafFeatureDriver));
        }
    }

    private boolean shouldRemoveNoContentCardOrZeroState() {
        if (mFeatureDrivers.isEmpty()) {
            return false;
        }

        if (!(mFeatureDrivers.get(0) instanceof NoContentDriver)
                && !(mFeatureDrivers.get(0) instanceof ZeroStateDriver)) {
            return false;
        }

        return mFeatureDrivers.size() > 2
                || (mFeatureDrivers.size() == 2 && !isLastDriverContinuationDriver());
    }

    private boolean isLastDriverContinuationDriver() {
        return !mFeatureDrivers.isEmpty()
                && mFeatureDrivers.get(mFeatureDrivers.size() - 1) instanceof ContinuationDriver;
    }

    /**
     * Removes the {@link FeatureDriver} represented by the {@link ModelChild} from all collections
     * containing it and updates any listening instances of {@link StreamContentListener} of the
     * removal.
     *
     * <p>Returns the index at which the {@link FeatureDriver} was removed, or -1 if it was not
     * found.
     */
    private int removeDriver(ModelChild modelChild) {
        FeatureDriver featureDriver = mModelChildFeatureDriverMap.get(modelChild);
        if (featureDriver == null) {
            Logger.w(TAG, "Attempting to remove feature from ModelChild not in map, %s",
                    modelChild.getContentId());
            return -1;
        }

        for (int i = 0; i < mFeatureDrivers.size(); i++) {
            if (mFeatureDrivers.get(i) == featureDriver) {
                mFeatureDrivers.remove(i);
                featureDriver.onDestroy();
                mModelChildFeatureDriverMap.remove(modelChild);
                notifyContentRemoved(i);
                return i;
            }
        }

        Logger.wtf(
                TAG, "Attempting to remove feature contained on map but not on list of children.");
        return -1;
    }

    private void notifyContentsAdded(int index, List<LeafFeatureDriver> leafFeatureDrivers) {
        if (mContentListener != null) {
            mContentListener.notifyContentsAdded(index, leafFeatureDrivers);
        }
    }

    private void notifyContentRemoved(int index) {
        if (mContentListener != null) {
            mContentListener.notifyContentRemoved(index);
        }
    }

    private void notifyContentsCleared() {
        if (mContentListener != null) {
            mContentListener.notifyContentsCleared();
        }
    }

    /** Dismisses all content and immediately shows a spinner. */
    public void showSpinner() {
        ZeroStateDriver zeroStateDriver = createSpinner();
        clearAllContent();
        mFeatureDrivers.add(zeroStateDriver);
        notifyContentsAdded(0, Collections.singletonList(zeroStateDriver));
    }

    /** Dismisses all content and shows the zero-state. */
    public void showZeroState(@ZeroStateShowReason int zeroStateShowReason) {
        ZeroStateDriver zeroStateDriver = createAndLogZeroState(zeroStateShowReason);
        clearAllContent();
        mFeatureDrivers.add(zeroStateDriver);
        notifyContentsAdded(0, Collections.singletonList(zeroStateDriver));
    }

    private void clearAllContent() {
        // TODO: Make sure to not notify listeners when driver is destroyed.
        for (FeatureDriver featureDriver : mFeatureDrivers) {
            featureDriver.onDestroy();
        }
        mFeatureDrivers.clear();
        notifyContentsCleared();
    }

    private ZeroStateDriver createAndLogZeroState(@ZeroStateShowReason int zeroStateShowReason) {
        mBasicLoggingApi.onZeroStateShown(zeroStateShowReason);

        return createZeroStateDriver();
    }

    @VisibleForTesting
    FeatureDriver createClusterDriver(ModelFeature modelFeature, int position) {
        return new ClusterDriver(mActionApi, mActionManager, mActionParserFactory, mBasicLoggingApi,
                modelFeature, mModelProvider, position, this, mStreamOfflineMonitor,
                mContentChangedListener, mContextMenuManager, mMainThreadRunner, mConfiguration,
                mViewLoggingUpdater, mTooltipApi);
    }

    @VisibleForTesting
    FeatureDriver createCardDriver(ModelFeature modelFeature, int position) {
        return new CardDriver(mActionApi, mActionManager, mActionParserFactory, mBasicLoggingApi,
                modelFeature, mModelProvider, position,
                (undoAction, pendingDismissCallback)
                        -> {
                    Logger.wtf(TAG, "Dismissing a card without a cluster is not supported.");
                },
                mStreamOfflineMonitor, mContentChangedListener, mContextMenuManager,
                mMainThreadRunner, mConfiguration, mViewLoggingUpdater, mTooltipApi);
    }

    @VisibleForTesting
    ContinuationDriver createContinuationDriver(BasicLoggingApi basicLoggingApi, Clock clock,
            Configuration configuration, Context context, ModelChild modelChild,
            ModelProvider modelProvider, int position, SnackbarApi snackbarApi, boolean restoring) {
        return new ContinuationDriver(basicLoggingApi, clock, configuration, context, this,
                modelChild, modelProvider, position, snackbarApi, mThreadUtils, restoring);
    }

    @VisibleForTesting
    NoContentDriver createNoContentDriver() {
        return new NoContentDriver();
    }

    @VisibleForTesting
    ZeroStateDriver createZeroStateDriver() {
        return new ZeroStateDriver(mBasicLoggingApi, mClock, mModelProvider,
                mContentChangedListener,
                /* spinnerShown = */ false);
    }

    @VisibleForTesting
    ZeroStateDriver createSpinner() {
        return new ZeroStateDriver(mBasicLoggingApi, mClock, mModelProvider,
                mContentChangedListener,
                /* spinnerShown= */ true);
    }

    public void setStreamContentListener(/*@Nullable*/ StreamContentListener contentListener) {
        this.mContentListener = contentListener;
    }

    @Override
    public void triggerPendingDismiss(String contentId, UndoAction undoAction,
            PendingDismissCallback pendingDismissCallback) {
        // Remove the feature driver to temporarily hide the card. Find the modelChild and index so
        // the driver can be recreated and inserted if dismiss is undone.
        ModelChild modelChild = getModelChildForContentId(contentId);
        if (modelChild == null) {
            Logger.wtf(TAG, "No model child found with that content id.");
            pendingDismissCallback.onDismissCommitted();
            return;
        }

        FeatureDriver featureDriver = mModelChildFeatureDriverMap.get(modelChild);
        if (featureDriver == null) {
            Logger.wtf(TAG, "No FeatureDriver found for that model child.");
            pendingDismissCallback.onDismissCommitted();
            return;
        }

        int index = mFeatureDrivers.indexOf(featureDriver);
        if (index < 0) {
            Logger.wtf(TAG, "No FeatureDriver found in the FeatureDriver list.");
            pendingDismissCallback.onDismissCommitted();
            return;
        }

        removeDriver(modelChild);
        addNoContentCardOrZeroStateIfNecessary(ZeroStateShowReason.CONTENT_DISMISSED);
        mSnackbarApi.show(undoAction.getConfirmationLabel(),
                undoAction.hasUndoLabel()
                        ? undoAction.getUndoLabel()
                        : mContext.getResources().getString(R.string.snackbar_default_action),
                new SnackbarCallbackApi() {
                    @Override
                    public void onDismissNoAction() {
                        pendingDismissCallback.onDismissCommitted();
                    }

                    @Override
                    public void onDismissedWithAction() {
                        alwaysRemoveNoContentOrZeroStateCardIfPresent();
                        pendingDismissCallback.onDismissReverted();
                        createFeatureChildAndInsertAtIndex(modelChild, index);
                    }
                });
    }

    private void createFeatureChildAndInsertAtIndex(ModelChild modelChild, int index) {
        FeatureDriver featureDriver = createFeatureChild(modelChild.getModelFeature(), index);
        if (featureDriver == null) {
            Logger.wtf(TAG, "Could not recreate the FeatureDriver.");
            return;
        }
        mFeatureDrivers.add(index, featureDriver);
        if (modelChild != null) {
            mModelChildFeatureDriverMap.put(modelChild, featureDriver);
        }
        LeafFeatureDriver leafFeatureDriver = featureDriver.getLeafFeatureDriver();
        if (leafFeatureDriver == null) {
            Logger.wtf(TAG, "No LeafFeatureDriver found.");
            return;
        }
        notifyContentsAdded(mFeatureDrivers.indexOf(featureDriver),
                Collections.singletonList(leafFeatureDriver));
    }

    /*@Nullable*/
    private ModelChild getModelChildForContentId(String contentId) {
        for (ModelChild model : mModelChildFeatureDriverMap.keySet()) {
            if (model.getContentId().equals(contentId)) {
                return model;
            }
        }
        return null;
    }

    /** Contains the {@link FeatureDriver} instances that were created and metadata about them. */
    private static class ChildCreationPayload {
        private final List<FeatureDriver> mFeatureDrivers;
        private final int mTokenCount;
        private final int mContentCount;

        private ChildCreationPayload(
                List<FeatureDriver> featureDrivers, int tokenCount, int contentCount) {
            this.mFeatureDrivers = featureDrivers;
            this.mTokenCount = tokenCount;
            this.mContentCount = contentCount;
        }
    }

    /** Allows listening for changes in the contents held by a {@link StreamDriver} */
    public interface StreamContentListener {
        /** Called when the given content has been added at the given index of stream content. */
        void notifyContentsAdded(int index, List<LeafFeatureDriver> newFeatureDrivers);

        /** Called when the content at the given index of stream content has been removed. */
        void notifyContentRemoved(int index);

        /** Called when the content in the stream has been cleared. */
        void notifyContentsCleared();
    }
}
