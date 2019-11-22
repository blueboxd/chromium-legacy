// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import com.google.android.libraries.feed.piet.host.CustomElementProvider;
import com.google.search.now.ui.piet.ElementsProto.CustomElementData;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link PietCustomElementProvider}. */
@RunWith(RobolectricTestRunner.class)
public class PietCustomElementProviderTest {
    @Mock
    private CustomElementProvider hostCustomElementProvider;

    private Context context;
    private View hostCreatedView;
    private PietCustomElementProvider customElementProvider;
    private PietCustomElementProvider delegatingCustomElementProvider;

    @Before
    public void setUp() {
        initMocks(this);

        context = Robolectric.buildActivity(Activity.class).get();
        hostCreatedView = new View(context);
        customElementProvider =
                new PietCustomElementProvider(context, /* customElementProvider */ null);
        delegatingCustomElementProvider =
                new PietCustomElementProvider(context, hostCustomElementProvider);
    }

    @Test
    public void testCreateCustomElement_noHostDelegate() {
        View view =
                customElementProvider.createCustomElement(CustomElementData.getDefaultInstance());
        assertThat(view).isNotNull();
    }

    @Test
    public void testCreateCustomElement_hostDelegate() {
        CustomElementData customElementData = CustomElementData.getDefaultInstance();
        when(hostCustomElementProvider.createCustomElement(customElementData))
                .thenReturn(hostCreatedView);

        View view = delegatingCustomElementProvider.createCustomElement(customElementData);
        assertThat(view).isEqualTo(hostCreatedView);
    }

    @Test
    public void testReleaseView_noHostDelegate() {
        customElementProvider.releaseCustomView(
                new View(context), CustomElementData.getDefaultInstance());

        // Above call should not throw
    }

    @Test
    public void testReleaseView_hostDelegate() {
        CustomElementData customElementData = CustomElementData.newBuilder().build();
        delegatingCustomElementProvider.releaseCustomView(hostCreatedView, customElementData);

        verify(hostCustomElementProvider).releaseCustomView(hostCreatedView, customElementData);
    }
}
