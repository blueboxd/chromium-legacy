// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.piet.host.CustomElementProvider;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.CustomElementData;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link PietCustomElementProvider}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PietCustomElementProviderTest {
    @Mock
    private CustomElementProvider mHostCustomElementProvider;

    private Context mContext;
    private View mHostCreatedView;
    private PietCustomElementProvider mCustomElementProvider;
    private PietCustomElementProvider mDelegatingCustomElementProvider;

    @Before
    public void setUp() {
        initMocks(this);

        mContext = Robolectric.buildActivity(Activity.class).get();
        mHostCreatedView = new View(mContext);
        mCustomElementProvider =
                new PietCustomElementProvider(mContext, /* customElementProvider */ null);
        mDelegatingCustomElementProvider =
                new PietCustomElementProvider(mContext, mHostCustomElementProvider);
    }

    @Test
    public void testCreateCustomElement_noHostDelegate() {
        View view =
                mCustomElementProvider.createCustomElement(CustomElementData.getDefaultInstance());
        assertThat(view).isNotNull();
    }

    @Test
    public void testCreateCustomElement_hostDelegate() {
        CustomElementData customElementData = CustomElementData.getDefaultInstance();
        when(mHostCustomElementProvider.createCustomElement(customElementData))
                .thenReturn(mHostCreatedView);

        View view = mDelegatingCustomElementProvider.createCustomElement(customElementData);
        assertThat(view).isEqualTo(mHostCreatedView);
    }

    @Test
    public void testReleaseView_noHostDelegate() {
        mCustomElementProvider.releaseCustomView(
                new View(mContext), CustomElementData.getDefaultInstance());

        // Above call should not throw
    }

    @Test
    public void testReleaseView_hostDelegate() {
        CustomElementData customElementData = CustomElementData.newBuilder().build();
        mDelegatingCustomElementProvider.releaseCustomView(mHostCreatedView, customElementData);

        verify(mHostCustomElementProvider).releaseCustomView(mHostCreatedView, customElementData);
    }
}
