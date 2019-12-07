// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.sessionmanager;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.KnownContent;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.api.internal.lifecycle.Resettable;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.functional.Function;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;

import java.util.List;
import java.util.Set;

/**
 * The FeedSessionManager is responsible for creating and updating. A session has a one-to-one
 * relationship with a ModelProvider. The FeedSessionManager implements the session functionality
 * and management.
 */
public interface FeedSessionManager extends Resettable {
    /**
     * This is called by the UI to get a new session. The life of the session is controlled by the
     * Session manager. The FeedSessionManager maintains HEAD$ which represents the most current
     * state of the stream. It will also decide which changes should be made to existing sessions
     * and the life time of existing sessions.
     */
    void getNewSession(ModelProvider modelProvider,
            /*@Nullable*/ ViewDepthProvider viewDepthProvider, UiContext uiContext);

    /**
     * Create a new Session attached to the ModelProvider for an existing sessionId. This will
     * invalidate any existing ModelProvider instances attached to the session.
     */
    void getExistingSession(String sessionId, ModelProvider modelProvider, UiContext uiContext);

    /** This is called by the ModelProvider when it is invalidated. */
    void invalidateSession(String sessionId);

    /** This is called by the ModelProvider to detach the ModelProvider for the session. */
    void detachSession(String sessionId);

    /**
     * This method will invalidate head. All {@link #getNewSession(ModelProvider, ViewDepthProvider,
     * UiContext)} will block until a new a response updates the FeedSessionManager providing the
     * new head.
     */
    void invalidateHead();

    /**
     * Method which causes a request to be created for the next page of content based upon the
     * continuation token. This could be called from multiple Model Providers.
     */
    void handleToken(String sessionId, StreamToken streamToken);

    /** Method which causes a refresh */
    void triggerRefresh(/*@Nullable*/ String sessionId);

    /**
     * Method which causes a refresh
     *
     * @param sessionId The session id associated with the request.
     * @param requestReason The reason for triggering a refresh.
     * @param uiContext The UI only context for the refresh.
     */
    void triggerRefresh(
            /*@Nullable*/ String sessionId, @RequestReason int requestReason, UiContext uiContext);

    /**
     * Returns a List of {@link StreamPayload} for each of the keys. This must be called on the
     * background thread.
     */
    Result<List<PayloadWithId>> getStreamFeatures(List<String> contentIds);

    /**
     * Return the shared state. This operation will be fast, so it can be called on the UI Thread.
     * This method will return {@code null} if the shared state is not found.
     */
    /*@Nullable*/
    StreamSharedState getSharedState(ContentId contentId);

    /**
     * Create a filtered {@link List} of {@code T} objects and return it through the {@link
     * Consumer}. The {@code filterPredicate} function will be applied to each node in the
     * structured tree defined by $HEAD. The method acts as both a transformer to create type {@code
     * T} and as a filter over the input {@link StreamPayload}. If {@code filterPredicate} returns
     * {@code null}, the value will not be added to the filtered list. This is an expensive
     * operation and will run on a background thread.
     */
    <T> void getStreamFeaturesFromHead(Function<StreamPayload, /*@Nullable*/ T> filterPredicate,
            Consumer<Result<List</*@NonNull*/ T>>> consumer);

    /** Sets {@link KnownContentListener} to allow for informing host of when content is added. */
    void setKnownContentListener(KnownContent.Listener knownContentListener);

    /**
     * Issues a request to record a set of actions, with the consumer being called back with the
     * resulting {@link ConsistencyToken}.
     */
    void triggerUploadActions(Set<StreamUploadableAction> actions);

    /**
     * Issues a request to record a set of actions, with the consumer being called back with the
     * resulting {@link ConsistencyToken}.
     */
    void fetchActionsAndUpload(Consumer<Result<ConsistencyToken>> consumer);

    /**
     * Return a {@link Consumer} which is able to handle an update to the SessionManager. An update
     * consists of {@link Model} containing a list of {@link StreamDataOperation} objects. The
     * {@link MutationContext} captures the context which is initiating the update operation.
     */
    Consumer<Result<Model>> getUpdateConsumer(MutationContext mutationContext);
}
