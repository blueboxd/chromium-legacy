// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedmodelprovider.internal;

import com.google.android.libraries.feed.api.internal.modelprovider.ModelToken;
import com.google.android.libraries.feed.api.internal.modelprovider.TokenCompletedObserver;
import com.google.android.libraries.feed.common.feedobservable.FeedObservable;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;

import java.util.ArrayList;
import java.util.List;

/** Implementation of the {@link ModelToken}. */
public final class UpdatableModelToken
        extends FeedObservable<TokenCompletedObserver> implements ModelToken {
    private final StreamToken token;
    private final boolean isSynthetic;

    public UpdatableModelToken(StreamToken token, boolean isSynthetic) {
        this.token = token;
        this.isSynthetic = isSynthetic;
    }

    @Override
    public boolean isSynthetic() {
        return isSynthetic;
    }

    @Override
    public StreamToken getStreamToken() {
        return token;
    }

    public List<TokenCompletedObserver> getObserversToNotify() {
        synchronized (observers) {
            return new ArrayList<>(observers);
        }
    }
}
