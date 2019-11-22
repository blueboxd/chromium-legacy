// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.drivers;

import static com.google.android.libraries.feed.common.Validators.checkNotNull;

import android.content.Context;
import android.support.annotation.VisibleForTesting;

import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;
import com.google.android.libraries.feed.api.host.action.ActionApi;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.logging.ZeroStateShowReason;
import com.google.android.libraries.feed.api.host.stream.SnackbarApi;
import com.google.android.libraries.feed.api.host.stream.SnackbarCallbackApi;
import com.google.android.libraries.feed.api.host.stream.TooltipApi;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionManager;
import com.google.android.libraries.feed.api.internal.actionparser.ActionParserFactory;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.knowncontent.FeedKnownContent;
import com.google.android.libraries.feed.api.internal.modelprovider.FeatureChange;
import com.google.android.libraries.feed.api.internal.modelprovider.FeatureChangeObserver;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild.Type;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.State;
import com.google.android.libraries.feed.basicstream.internal.drivers.ContinuationDriver.CursorChangedListener;
import com.google.android.libraries.feed.basicstream.internal.pendingdismiss.PendingDismissHandler;
import com.google.android.libraries.feed.basicstream.internal.scroll.BasicStreamScrollMonitor;
import com.google.android.libraries.feed.basicstream.internal.scroll.BasicStreamScrollTracker;
import com.google.android.libraries.feed.basicstream.internal.scroll.ScrollRestorer;
import com.google.android.libraries.feed.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.sharedstream.contextmenumanager.ContextMenuManager;
import com.google.android.libraries.feed.sharedstream.offlinemonitor.StreamOfflineMonitor;
import com.google.android.libraries.feed.sharedstream.pendingdismiss.PendingDismissCallback;
import com.google.android.libraries.feed.sharedstream.proto.UiRefreshReasonProto.UiRefreshReason;
import com.google.android.libraries.feed.sharedstream.proto.UiRefreshReasonProto.UiRefreshReason.Reason;
import com.google.android.libraries.feed.sharedstream.removetrackingfactory.StreamRemoveTrackingFactory;
import com.google.android.libraries.feed.sharedstream.scroll.ScrollLogger;
import com.google.android.libraries.feed.sharedstream.scroll.ScrollTracker;
import com.google.search.now.ui.action.FeedActionProto.UndoAction;

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
    private final ActionApi actionApi;
    private final ActionManager actionManager;
    private final ActionParserFactory actionParserFactory;
    private final ThreadUtils threadUtils;
    private final ModelProvider modelProvider;
    private final Map<ModelChild, FeatureDriver> modelChildFeatureDriverMap;
    private final List<FeatureDriver> featureDrivers;
    private final Clock clock;
    private final Configuration configuration;
    private final Context context;
    private final SnackbarApi snackbarApi;
    private final ContentChangedListener contentChangedListener;
    private final ScrollRestorer scrollRestorer;
    private final BasicLoggingApi basicLoggingApi;
    private final StreamOfflineMonitor streamOfflineMonitor;
    private final ContextMenuManager contextMenuManager;
    private final boolean isInitialLoad;
    private final MainThreadRunner mainThreadRunner;
    private final ViewLoggingUpdater viewLoggingUpdater;
    private final TooltipApi tooltipApi;
    private final UiRefreshReason uiRefreshReason;
    private final ScrollTracker scrollTracker;

    private boolean restoring;
    private boolean rootFeatureConsumed;
    private boolean modelFeatureChangeObserverRegistered;
    /*@Nullable*/ private StreamContentListener contentListener;

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
        this.actionApi = actionApi;
        this.actionManager = actionManager;
        this.actionParserFactory = actionParserFactory;
        this.threadUtils = threadUtils;
        this.modelProvider = modelProvider;
        this.clock = clock;
        this.context = context;
        this.snackbarApi = snackbarApi;
        this.contextMenuManager = contextMenuManager;
        this.modelChildFeatureDriverMap = new HashMap<>();
        this.featureDrivers = new ArrayList<>();
        this.configuration = configuration;
        this.contentChangedListener = contentChangedListener;
        this.scrollRestorer = scrollRestorer;
        this.basicLoggingApi = basicLoggingApi;
        this.streamOfflineMonitor = streamOfflineMonitor;
        this.restoring = restoring;
        this.isInitialLoad = isInitialLoad;
        this.mainThreadRunner = mainThreadRunner;
        this.viewLoggingUpdater = viewLoggingUpdater;
        this.tooltipApi = tooltipApi;
        this.uiRefreshReason = uiRefreshReason;
        scrollTracker = new BasicStreamScrollTracker(
                mainThreadRunner, new ScrollLogger(basicLoggingApi), clock, scrollMonitor);

        modelProvider.enableRemoveTracking(
                new StreamRemoveTrackingFactory(modelProvider, feedKnownContent));
    }

    /**
     * Returns a the list of {@link LeafFeatureDriver} instances for the children generated from the
     * given {@link ModelFeature}.
     */
    public List<LeafFeatureDriver> getLeafFeatureDrivers() {
        if (modelProvider.getCurrentState() == State.READY && !rootFeatureConsumed) {
            ChildCreationPayload childCreationPayload;
            rootFeatureConsumed = true;
            ModelFeature rootFeature = modelProvider.getRootFeature();
            if (rootFeature != null) {
                childCreationPayload = createAndInsertChildren(rootFeature, modelProvider);
                rootFeature.registerObserver(this);
                modelFeatureChangeObserverRegistered = true;
                if (uiRefreshReason.getReason().equals(Reason.ZERO_STATE)) {
                    basicLoggingApi.onZeroStateRefreshCompleted(
                            childCreationPayload.contentCount, childCreationPayload.tokenCount);
                }
            } else {
                basicLoggingApi.onInternalError(InternalFeedError.NO_ROOT_FEATURE);
                Logger.w(TAG, "found null root feature loading Leaf Feature Drivers");
                if (uiRefreshReason.getReason().equals(Reason.ZERO_STATE)) {
                    basicLoggingApi.onZeroStateRefreshCompleted(
                            /* newContentCount= */ 0, /* newTokenCount= */ 0);
                }
            }
        }

        if (!isInitialLoad) {
            addNoContentCardOrZeroStateIfNecessary(ZeroStateShowReason.NO_CONTENT);
        }

        return buildLeafFeatureDrivers(featureDrivers);
    }

    public void maybeRestoreScroll() {
        if (!restoring) {
            return;
        }

        if (isLastDriverContinuationDriver()) {
            ContinuationDriver continuationDriver =
                    (ContinuationDriver) featureDrivers.get(featureDrivers.size() - 1);
            // If there is a synthetic token, we should only restore if it has not been handled yet.
            if (continuationDriver.hasTokenBeenHandled()) {
                return;
            }
        }

        restoring = false;

        scrollRestorer.maybeRestoreScroll();
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
            /*@Nullable*/ private ModelChild next;

            @Override
            public boolean hasNext() {
                next = streamCursor.getNextItem();
                return next != null;
            }

            @Override
            public ModelChild next() {
                return checkNotNull(next);
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
                featureDrivers.add(insertionIndex, featureDriverChild);
                modelChildFeatureDriverMap.put(child, featureDriverChild);
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
                        createContinuationDriver(basicLoggingApi, clock, configuration, context,
                                child, modelProvider, position, snackbarApi, restoring);

                // TODO: Look into moving initialize() into a more generic location. We don't
                // really want work to be done in the constructor so we call an initialize() method
                // to kick off any expensive work the driver may need to do.
                continuationDriver.initialize();
                return continuationDriver;
            case Type.UNBOUND:
                basicLoggingApi.onInternalError(InternalFeedError.TOP_LEVEL_UNBOUND_CHILD);
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

        basicLoggingApi.onInternalError(InternalFeedError.TOP_LEVEL_INVALID_FEATURE_TYPE);
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
                basicLoggingApi.onInternalError(InternalFeedError.FAILED_TO_CREATE_LEAF);
                removes.add(featureDriver);
            }
        }
        for (FeatureDriver driver : removes) {
            this.featureDrivers.remove(driver);
            driver.onDestroy();
        }

        streamOfflineMonitor.requestOfflineStatusForNewContent();

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
            int insertionIndex = featureDrivers.size();

            notifyContentsAdded(insertionIndex,
                    buildLeafFeatureDrivers(createAndInsertChildrenAtIndex(
                            appendedChildren, modelProvider, insertionIndex)
                                                    .featureDrivers));
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
                createAndInsertChildrenAtIndex(modelChildren, modelProvider, continuationIndex);

        List<FeatureDriver> newChildren = tokenPayload.featureDrivers;

        basicLoggingApi.onTokenCompleted(
                wasSynthetic, tokenPayload.contentCount, tokenPayload.tokenCount);
        notifyContentsAdded(continuationIndex, buildLeafFeatureDrivers(newChildren));
        maybeRemoveNoContentOrZeroStateCard();
        // Swap no content card with zero state if there are no more drivers.
        if (newChildren.isEmpty() && featureDrivers.size() == 1
                && featureDrivers.get(0) instanceof NoContentDriver) {
            showZeroState(ZeroStateShowReason.NO_CONTENT_FROM_CONTINUATION_TOKEN);
        }

        maybeRestoreScroll();
    }

    private void alwaysRemoveNoContentOrZeroStateCardIfPresent() {
        if (featureDrivers.size() == 1
                && ((featureDrivers.get(0) instanceof NoContentDriver)
                        || (featureDrivers.get(0) instanceof ZeroStateDriver))) {
            featureDrivers.get(0).onDestroy();
            featureDrivers.remove(0);
            notifyContentRemoved(0);
        }
    }

    private void maybeRemoveNoContentOrZeroStateCard() {
        if (shouldRemoveNoContentCardOrZeroState()) {
            featureDrivers.get(0).onDestroy();
            featureDrivers.remove(0);
            notifyContentRemoved(0);
        }
    }

    public void onDestroy() {
        for (FeatureDriver featureDriver : featureDrivers) {
            featureDriver.onDestroy();
        }
        ModelFeature modelFeature = modelProvider.getRootFeature();
        if (modelFeature != null && modelFeatureChangeObserverRegistered) {
            modelFeature.unregisterObserver(this);
            modelFeatureChangeObserverRegistered = false;
        }
        featureDrivers.clear();
        modelChildFeatureDriverMap.clear();
        scrollTracker.onUnbind();
    }

    public boolean hasContent() {
        if (featureDrivers.isEmpty()) {
            return false;
        }
        return !(featureDrivers.get(0) instanceof NoContentDriver)
                && !(featureDrivers.get(0) instanceof ZeroStateDriver);
    }

    public boolean isZeroStateBeingShown() {
        return !featureDrivers.isEmpty() && featureDrivers.get(0) instanceof ZeroStateDriver
                && !((ZeroStateDriver) featureDrivers.get(0)).isSpinnerShowing();
    }

    public void setModelProviderForZeroState(ModelProvider modelProvider) {
        if (!isZeroStateBeingShown()) {
            Logger.wtf(TAG,
                    "setModelProviderForZeroState should only be called when zero state is shown");
            return;
        }
        ((ZeroStateDriver) featureDrivers.get(0)).setModelProvider(modelProvider);
    }

    private void addNoContentCardOrZeroStateIfNecessary(
            @ZeroStateShowReason int zeroStateShowReason) {
        LeafFeatureDriver leafFeatureDriver = null;
        if (!restoring && featureDrivers.isEmpty()) {
            leafFeatureDriver = createAndLogZeroState(zeroStateShowReason);
        } else if (featureDrivers.size() == 1 && isLastDriverContinuationDriver()) {
            leafFeatureDriver = createNoContentDriver();
        }

        if (leafFeatureDriver != null) {
            featureDrivers.add(0, leafFeatureDriver);
            notifyContentsAdded(0, Collections.singletonList(leafFeatureDriver));
        }
    }

    private boolean shouldRemoveNoContentCardOrZeroState() {
        if (featureDrivers.isEmpty()) {
            return false;
        }

        if (!(featureDrivers.get(0) instanceof NoContentDriver)
                && !(featureDrivers.get(0) instanceof ZeroStateDriver)) {
            return false;
        }

        return featureDrivers.size() > 2
                || (featureDrivers.size() == 2 && !isLastDriverContinuationDriver());
    }

    private boolean isLastDriverContinuationDriver() {
        return !featureDrivers.isEmpty()
                && featureDrivers.get(featureDrivers.size() - 1) instanceof ContinuationDriver;
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
        FeatureDriver featureDriver = modelChildFeatureDriverMap.get(modelChild);
        if (featureDriver == null) {
            Logger.w(TAG, "Attempting to remove feature from ModelChild not in map, %s",
                    modelChild.getContentId());
            return -1;
        }

        for (int i = 0; i < featureDrivers.size(); i++) {
            if (featureDrivers.get(i) == featureDriver) {
                featureDrivers.remove(i);
                featureDriver.onDestroy();
                modelChildFeatureDriverMap.remove(modelChild);
                notifyContentRemoved(i);
                return i;
            }
        }

        Logger.wtf(
                TAG, "Attempting to remove feature contained on map but not on list of children.");
        return -1;
    }

    private void notifyContentsAdded(int index, List<LeafFeatureDriver> leafFeatureDrivers) {
        if (contentListener != null) {
            contentListener.notifyContentsAdded(index, leafFeatureDrivers);
        }
    }

    private void notifyContentRemoved(int index) {
        if (contentListener != null) {
            contentListener.notifyContentRemoved(index);
        }
    }

    private void notifyContentsCleared() {
        if (contentListener != null) {
            contentListener.notifyContentsCleared();
        }
    }

    /** Dismisses all content and immediately shows a spinner. */
    public void showSpinner() {
        ZeroStateDriver zeroStateDriver = createSpinner();
        clearAllContent();
        featureDrivers.add(zeroStateDriver);
        notifyContentsAdded(0, Collections.singletonList(zeroStateDriver));
    }

    /** Dismisses all content and shows the zero-state. */
    public void showZeroState(@ZeroStateShowReason int zeroStateShowReason) {
        ZeroStateDriver zeroStateDriver = createAndLogZeroState(zeroStateShowReason);
        clearAllContent();
        featureDrivers.add(zeroStateDriver);
        notifyContentsAdded(0, Collections.singletonList(zeroStateDriver));
    }

    private void clearAllContent() {
        // TODO: Make sure to not notify listeners when driver is destroyed.
        for (FeatureDriver featureDriver : featureDrivers) {
            featureDriver.onDestroy();
        }
        featureDrivers.clear();
        notifyContentsCleared();
    }

    private ZeroStateDriver createAndLogZeroState(@ZeroStateShowReason int zeroStateShowReason) {
        basicLoggingApi.onZeroStateShown(zeroStateShowReason);

        return createZeroStateDriver();
    }

    @VisibleForTesting
    FeatureDriver createClusterDriver(ModelFeature modelFeature, int position) {
        return new ClusterDriver(actionApi, actionManager, actionParserFactory, basicLoggingApi,
                modelFeature, modelProvider, position, this, streamOfflineMonitor,
                contentChangedListener, contextMenuManager, mainThreadRunner, configuration,
                viewLoggingUpdater, tooltipApi);
    }

    @VisibleForTesting
    FeatureDriver createCardDriver(ModelFeature modelFeature, int position) {
        return new CardDriver(actionApi, actionManager, actionParserFactory, basicLoggingApi,
                modelFeature, modelProvider, position,
                (undoAction, pendingDismissCallback)
                        -> {
                    Logger.wtf(TAG, "Dismissing a card without a cluster is not supported.");
                },
                streamOfflineMonitor, contentChangedListener, contextMenuManager, mainThreadRunner,
                configuration, viewLoggingUpdater, tooltipApi);
    }

    @VisibleForTesting
    ContinuationDriver createContinuationDriver(BasicLoggingApi basicLoggingApi, Clock clock,
            Configuration configuration, Context context, ModelChild modelChild,
            ModelProvider modelProvider, int position, SnackbarApi snackbarApi, boolean restoring) {
        return new ContinuationDriver(basicLoggingApi, clock, configuration, context, this,
                modelChild, modelProvider, position, snackbarApi, threadUtils, restoring);
    }

    @VisibleForTesting
    NoContentDriver createNoContentDriver() {
        return new NoContentDriver();
    }

    @VisibleForTesting
    ZeroStateDriver createZeroStateDriver() {
        return new ZeroStateDriver(basicLoggingApi, clock, modelProvider, contentChangedListener,
                /* spinnerShown = */ false);
    }

    @VisibleForTesting
    ZeroStateDriver createSpinner() {
        return new ZeroStateDriver(basicLoggingApi, clock, modelProvider, contentChangedListener,
                /* spinnerShown= */ true);
    }

    public void setStreamContentListener(/*@Nullable*/ StreamContentListener contentListener) {
        this.contentListener = contentListener;
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

        FeatureDriver featureDriver = modelChildFeatureDriverMap.get(modelChild);
        if (featureDriver == null) {
            Logger.wtf(TAG, "No FeatureDriver found for that model child.");
            pendingDismissCallback.onDismissCommitted();
            return;
        }

        int index = featureDrivers.indexOf(featureDriver);
        if (index < 0) {
            Logger.wtf(TAG, "No FeatureDriver found in the FeatureDriver list.");
            pendingDismissCallback.onDismissCommitted();
            return;
        }

        removeDriver(modelChild);
        addNoContentCardOrZeroStateIfNecessary(ZeroStateShowReason.CONTENT_DISMISSED);
        snackbarApi.show(undoAction.getConfirmationLabel(),
                undoAction.hasUndoLabel()
                        ? undoAction.getUndoLabel()
                        : context.getResources().getString(R.string.snackbar_default_action),
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
        featureDrivers.add(index, featureDriver);
        if (modelChild != null) {
            modelChildFeatureDriverMap.put(modelChild, featureDriver);
        }
        LeafFeatureDriver leafFeatureDriver = featureDriver.getLeafFeatureDriver();
        if (leafFeatureDriver == null) {
            Logger.wtf(TAG, "No LeafFeatureDriver found.");
            return;
        }
        notifyContentsAdded(featureDrivers.indexOf(featureDriver),
                Collections.singletonList(leafFeatureDriver));
    }

    /*@Nullable*/
    private ModelChild getModelChildForContentId(String contentId) {
        for (ModelChild model : modelChildFeatureDriverMap.keySet()) {
            if (model.getContentId().equals(contentId)) {
                return model;
            }
        }
        return null;
    }

    /** Contains the {@link FeatureDriver} instances that were created and metadata about them. */
    private static class ChildCreationPayload {
        private final List<FeatureDriver> featureDrivers;
        private final int tokenCount;
        private final int contentCount;

        private ChildCreationPayload(
                List<FeatureDriver> featureDrivers, int tokenCount, int contentCount) {
            this.featureDrivers = featureDrivers;
            this.tokenCount = tokenCount;
            this.contentCount = contentCount;
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
