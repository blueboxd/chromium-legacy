// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.store;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.SemanticPropertiesWithId;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.feedobservable.Observable;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamLocalAction;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;

import java.util.List;
import java.util.Set;

/**
 * Interface defining the Store API used by the feed library to persist the internal proto objects
 * into a an underlying store. The store is divided into content and structure portions. Content is
 * retrieved using String representing the {@code ContentId}. The structure is defined through a set
 * of {@link StreamDataOperation} which define the structure of the stream. $HEAD defines the full
 * set of {@code StreamDataOperation}s defining the full stream. A {@link
 * org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSession}
 * represents an instance of the stream defined by a possible subset of $HEAD.
 *
 * <p>This object should not be used on the UI thread, as it may block on slow IO operations.
 */
public interface Store extends Observable<StoreListener> {
    /** The session id representing $HEAD */
    String HEAD_SESSION_ID = "$HEAD";

    /**
     * Returns an {@code List} of the {@link PayloadWithId}s for the ContentId Strings passed in
     * through {@code contentIds}.
     */
    Result<List<PayloadWithId>> getPayloads(List<String> contentIds);

    /** Get the {@link StreamSharedState}s stored as content. */
    Result<List<StreamSharedState>> getSharedStates();

    /**
     * Returns a list of all {@link StreamStructure}s for the session. The Stream Structure does not
     * contain the content. This represents the full structure of the stream for a particular
     * session.
     */
    Result<List<StreamStructure>> getStreamStructures(String sessionId);

    /** Returns a list of all the session ids, excluding the $HEAD session. */
    Result<List<String>> getAllSessions();

    /** Gets all semantic properties objects associated with a given list of contentIds */
    Result<List<SemanticPropertiesWithId>> getSemanticProperties(List<String> contentIds);

    /**
     * Gets an set of ALL {@link UploadableStreamAction}. Note that this includes expired actions.
     */
    Result<Set<StreamUploadableAction>> getAllUploadableActions();

    /**
     * Gets an increasing, time-ordered list of ALL {@link StreamLocalAction} dismisses. Note that
     * this includes expired actions.
     */
    Result<List<StreamLocalAction>> getAllDismissLocalActions();

    /**
     * Create a new session and return the id. The session is defined by all the {@link
     * StreamDataOperation}s which are currently stored in $HEAD.
     */
    Result<String> createNewSession();

    /** Remove a specific session. It is illegal to attempt to remove $HEAD. */
    void removeSession(String sessionId);

    /**
     * Clears out all data operations defining $HEAD. Only $HEAD supports reset, all other session
     * may not be reset. Instead they should be invalidated and removed, then a new session is
     * created from $HEAD.
     */
    void clearHead();

    /** Returns a mutation used to modify the content in the persistent store. */
    ContentMutation editContent();

    /** Returns a session mutation applied to a Session. */
    SessionMutation editSession(String sessionId);

    /** Returns a semantic properties mutation used to modify the properties in the store */
    SemanticPropertiesMutation editSemanticProperties();

    /** Returns an action mutation used to modify actions in the store */
    LocalActionMutation editLocalActions();

    /** Returns an action mutation used to modify uploadable actions in the store */
    UploadableActionMutation editUploadableActions();

    /**
     * Returns a runnable which will garbage collect the persisted content. This is called by the
     * SessionManager which controls the scheduling of cleanup tasks.
     */
    Runnable triggerContentGc(Set<String> reservedContentIds,
            Supplier<Set<String>> accessibleContent, boolean keepSharedStates);

    /**
     * Returns a runnable that will garbage collect actions. All valid actions will be copied over
     * to a new copy of the journal.
     */
    Runnable triggerLocalActionGc(List<StreamLocalAction> actions, List<String> validContentIds);

    /**
     * Switches to an ephemeral version of the store. Ephemeral mode of the store should run in
     * memory and not attempt to write to disk.
     */
    void switchToEphemeralMode();

    /** Whether the store is in ephemeral mode */
    boolean isEphemeralMode();
}
