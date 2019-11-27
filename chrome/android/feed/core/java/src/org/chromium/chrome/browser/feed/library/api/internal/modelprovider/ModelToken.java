// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.modelprovider;

import org.chromium.chrome.browser.feed.library.common.feedobservable.Observable;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;

/** Defines the behavior of a Continuation Token. */
public interface ModelToken extends Observable<TokenCompletedObserver> {
    /** Returns the {@link StreamToken} proto instance. */
    StreamToken getStreamToken();

    /** Returns {@code true} if token was generated on-device and can complete quickly. */
    boolean isSynthetic();
}
