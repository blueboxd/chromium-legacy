// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;

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
