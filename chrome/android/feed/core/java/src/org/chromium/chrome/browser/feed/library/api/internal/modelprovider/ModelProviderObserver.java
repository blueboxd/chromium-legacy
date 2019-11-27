// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.modelprovider;

import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;

/** Interface used to observe events on the {@link ModelProvider}. */
public interface ModelProviderObserver {
    /**
     * This event is triggered when the ModelProvider is fully initialized and it can be accessed by
     * the UI. If you register for the ModelProvider after the Session is Ready, we will call the
     * Observer. Otherwise the Observer is called once initialization is finished.
     */
    void onSessionStart(UiContext uiContext);

    /**
     * This event is triggered when the Session is invalidated. Once this is called, the UI should
     * no longer call this model. The ModelProvider will free all resources assocated with it
     * including invalidating all existing Cursors.
     *
     * @param uiContext If the session is being finished because of the UI, then this will be the
     *     context given by the UI, otherwise it will be {@link UiContext#getDefaultInstance()}.
     */
    void onSessionFinished(UiContext uiContext);

    /**
     * This is called in the event of an error. For example, if we are making a request and it fails
     * due to network connectivity issues, this event will indicate the error.
     */
    void onError(ModelError modelError);
}
