// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.modelprovider;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.common.feedobservable.Observable;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;

import java.util.List;

/**
 * A ModelProvider provides access to the Stream Model for the UI layer. It is populated by the
 * Session Manager. The ModelProvider is backed by a Session instance, there is a one-to-one
 * relationship between the ModelProvider and Session implementation. The Stream Library uses the
 * model to build the UI displayed to the user.
 */
public interface ModelProvider extends Observable<ModelProviderObserver> {
    /**
     * Provides the contentId of the lowest child of the root viewed by the user, or {@code null} if
     * no content with a ContentId has been seen.
     */
    interface ViewDepthProvider {
        /** Returns a contentId of the lowest root child seen by the user. */
        @Nullable
        String getChildViewDepth();
    }

    /** Returns a Mutator used to change the model. */
    ModelMutation edit();

    /**
     * This is called to invalidate the model. The SessionManager calls this to free all resources
     * held by this ModelProvider instance.
     */
    void invalidate();

    /**
     * This is called to invalidate the model. The SessionManager calls this to free all resources
     * held by this ModelProvider instance.
     */
    void invalidate(UiContext uiContext);

    /**
     * This is called to detach the ModelProvider from the SessionManager. The Session will continue
     * to be persisted, but the SessionManager will drop connections to this ModelProvider. The
     * ModelProvider will enter a {@code State.INVALIDATED}. This will not call the {@link
     * ModelProviderObserver#onSessionFinished(UiContext)}.
     */
    void detachModelProvider();

    /** Used by the session to raises a {@link ModelError} with the ModelProvider. */
    void raiseError(ModelError error);

    /**
     * Returns a {@code ModelFeature} which represents the root of the stream tree. Returns {@code
     * null} if the stream is empty.
     */
    @Nullable
    ModelFeature getRootFeature();

    /** Return a ModelChild for a String ContentId */
    @Nullable
    ModelChild getModelChild(String contentId);

    /**
     * Returns a {@code StreamSharedState} containing shared state such as the Piet shard state.
     * Returns {@code null} if the shared state is not found.
     */
    @Nullable
    StreamSharedState getSharedState(ContentId contentId);

    /**
     * Handle the processing of a {@code ModelToken}. For example start a request for the next page
     * of content. The results of handling the token will be available through Observers on the
     * ModelToken. Returns {@literal false} if a {@code modelToken} is not recognized and observers
     * will not be fired.
     */
    boolean handleToken(ModelToken modelToken);

    /**
     * Allow the stream to force a refresh. This will result in the current model being invalidated
     * if the requested refresh is successful.
     *
     * @param requestReason The reason for this refresh.
     */
    void triggerRefresh(@RequestReason int requestReason);

    /**
     * Allow the stream to force a refresh. This will result in the current model being invalidated
     * if the requested refresh is successful.
     *
     * @param requestReason The reason for this refresh.
     * @param uiContext The {@link UiContext} that caused this mutation. Will be round-tripped to
     *     {@link ModelProviderObserver#onSessionFinished(UiContext)}
     */
    void triggerRefresh(@RequestReason int requestReason, UiContext uiContext);

    /** Defines the Lifecycle of the ModelProvider */
    @IntDef({State.INITIALIZING, State.READY, State.INVALIDATED})
    @interface State {
        /**
         * State of the Model Provider before it has been fully initialized. The model shouldn't be
         * accessed before it enters the {@code READY} state. You should register an Observer to
         * receive an event when the model is ready.
         */
        int INITIALIZING = 0;
        /** State of the Model Provider when it is ready for normal use. */
        int READY = 1;
        /**
         * State of the Model Provider when it has been invalidated. In this mode, the Model is no
         * longer valid and methods will fail.
         */
        int INVALIDATED = 2;
    }

    /** Returns the current state of the ModelProvider */
    @State
    int getCurrentState();

    /** A String which represents the session bound to the ModelProvider. */
    @Nullable
    String getSessionId();

    /**
     * Returns a List of ModelChild for the root. These children may not be bound. This is not
     * intended to be used by the Stream to access children directly. Instead the ModelCursor should
     * be used.
     */
    List<ModelChild> getAllRootChildren();

    /** Allow the Stream to provide a RemoveTracking based upon mutation context. */
    interface RemoveTrackingFactory<T> {
        /**
         * Returns the {@link RemoveTracking}, if this returns {@code null} then no remove tracking
         * will be preformed on this ModelProvider mutation.
         */
        @Nullable
        RemoveTracking<T> create(MutationContext mutationContext);
    }

    /** Called by the stream to set the {@link RemoveTrackingFactory}. */
    void enableRemoveTracking(RemoveTrackingFactory<?> removeTrackingFactory);
}
