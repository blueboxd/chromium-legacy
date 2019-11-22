// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.internal.modelprovider;

import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.android.libraries.feed.common.functional.Predicate;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

/** Factory method for creating {@link ModelProvider} instances. */
public interface ModelProviderFactory {
    /**
     * Returns a new instance of a {@link ModelProvider} from an existing session. This session may
     * become INVALIDATED if the session was garbage collected by the Session Manager.
     */
    ModelProvider create(String sessionId, UiContext uiContext);

    /** Returns a new instance of a {@link ModelProvider} from $HEAD. */
    ModelProvider createNew(/*@Nullable*/ ViewDepthProvider viewDepthProvider, UiContext uiContext);

    ModelProvider createNew(
            /*@Nullable*/ ViewDepthProvider viewDepthProvider,
            /*@Nullable*/ Predicate<StreamStructure> filterPredicate, UiContext uiContext);
}
