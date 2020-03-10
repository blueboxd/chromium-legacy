// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.content.Intent;
import android.content.pm.ResolveInfo;

import java.util.List;

/**
 * This interface allows the embedder to supply the logic that ExternalNavigationHandler.java uses
 * to determine effective navigation/redirect while handling a given intent.
 */
public interface RedirectHandler {
    /**
     * @return whether on effective intent redirect chain or not.
     */
    boolean isOnEffectiveIntentRedirectChain();

    /**
     * @param hasExternalProtocol whether the destination URI has an external protocol or not.
     * @param isForTrustedCallingApp whether the app we would launch to is trusted and what launched
     *         this embedder.
     * @return whether we should stay in this app or not.
     */
    boolean shouldStayInApp(boolean hasExternalProtocol, boolean isForTrustedCallingApp);

    /**
     * @return Whether the current navigation is of the type that should always stay in this app.
     */
    boolean shouldNavigationTypeStayInApp();

    /**
     * @return Whether this navigation is initiated by a Custom Tab {@link Intent}.
     */
    boolean isFromCustomTabIntent();

    /**
     * @return whether navigation is from a user's typing or not.
     */
    boolean isNavigationFromUserTyping();

    /**
     * Will cause shouldNotOverrideUrlLoading() to return true until a new user-initiated navigation
     * occurs. The embedder implementation is responsible for enforcing these semantics.
     */
    void setShouldNotOverrideUrlLoadingOnCurrentRedirectChain();

    /**
     * @return whether we should stay in this app or not.
     */
    boolean shouldNotOverrideUrlLoading();

    /**
     * @return whether |resolvingInfos| contains a new resolver for the intent on which this
     * redirect handler was operating, compared to the resolvers shown when the user chose this app
     * to handle that intent. Relevant only if this app is one that handles incoming user
     * intents.
     */
    boolean hasNewResolver(List<ResolveInfo> resolvingInfos);
}
