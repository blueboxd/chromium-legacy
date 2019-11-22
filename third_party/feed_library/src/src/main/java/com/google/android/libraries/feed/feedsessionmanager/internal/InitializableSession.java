// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedsessionmanager.internal;

import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

import java.util.List;

/** A {@link Session} which supports initialization from $HEAD. */
public interface InitializableSession extends Session {
    /**
     * Bind a ModelProvider to an existing, unbound session. If {@code null} is passed, this will
     * unbind the Session, removing all references to the ModelProvider.
     */
    void bindModelProvider(
            /*@Nullable*/ ModelProvider modelProvider,
            /*@Nullable*/ ViewDepthProvider viewDepthProvider);

    /** Set the session id. */
    void setSessionId(String sessionId);

    /** Called to initialize the session from $HEAD */
    void populateModelProvider(List<StreamStructure> session, boolean cachedBindings,
            boolean legacyHeadContent, UiContext uiContext);
}
