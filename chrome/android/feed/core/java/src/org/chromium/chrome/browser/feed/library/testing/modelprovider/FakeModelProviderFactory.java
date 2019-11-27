// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.modelprovider;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.common.functional.Predicate;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;

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
