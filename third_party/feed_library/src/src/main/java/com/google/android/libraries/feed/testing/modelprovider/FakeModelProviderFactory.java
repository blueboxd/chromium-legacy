// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.modelprovider;

import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.common.functional.Predicate;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

/**
 * Fake for tests using {@link ModelProviderFactory}. Functionality should be added to this class as
 * needed.
 */
public class FakeModelProviderFactory implements ModelProviderFactory {
    private FakeModelProvider mModelProvider;
    private String mCreateSessionId;
    /*@Nullable*/ private ViewDepthProvider mViewDepthProvider;

    @Override
    public ModelProvider create(String sessionId, UiContext uiContext) {
        this.mModelProvider = new FakeModelProvider();
        this.mCreateSessionId = sessionId;
        return mModelProvider;
    }

    @Override
    public ModelProvider createNew(
            /*@Nullable*/ ViewDepthProvider viewDepthProvider, UiContext uiContext) {
        this.mViewDepthProvider = viewDepthProvider;
        this.mModelProvider = new FakeModelProvider();
        return mModelProvider;
    }

    @Override
    public ModelProvider createNew(
            /*@Nullable*/ ViewDepthProvider viewDepthProvider,
            /*@Nullable*/ Predicate<StreamStructure> filterPredicate, UiContext uiContext) {
        this.mViewDepthProvider = viewDepthProvider;
        this.mModelProvider = new FakeModelProvider();
        return mModelProvider;
    }

    public FakeModelProvider getLatestModelProvider() {
        return mModelProvider;
    }

    public String getLatestCreateSessionId() {
        return mCreateSessionId;
    }

    /*@Nullable*/
    public ViewDepthProvider getViewDepthProvider() {
        return mViewDepthProvider;
    }
}
