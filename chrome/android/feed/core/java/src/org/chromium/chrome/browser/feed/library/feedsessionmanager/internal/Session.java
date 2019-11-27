// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;

import java.util.List;
import java.util.Set;

/** A {@code Session} is a instance which supports update and optionally a {@link ModelProvider}. */
public interface Session {
    /**
     * Called to update the Session, this is responsible for updating the Stored session instance.
     */
    void updateSession(boolean clearHead, List<StreamStructure> streamStructures, int schemaVersion,
            /*@Nullable*/ MutationContext mutationContext);

    boolean invalidateOnResetHead();

    /** The unique identifier created by the storage layer for the session. */
    String getSessionId();

    /** Returns a ModelProvider for a Session, if one is defined. */
    /*@Nullable*/
    ModelProvider getModelProvider();

    /** Returns an unmodifiable set of the StreamContentIds used by the session. */
    Set<String> getContentInSession();
}
