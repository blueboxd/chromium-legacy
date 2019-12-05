// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link NoContentViewHolder}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NoContentViewHolderTest {
    private NoContentViewHolder mNoContentViewHolder;
    private FrameLayout mFrameLayout;

    ColorDrawable mRedBackground = new ColorDrawable(Color.RED);

    @Mock
    private CardConfiguration mCardConfiguration;

    @Before
    public void setup() {
        initMocks(this);
        Context context = Robolectric.buildActivity(Activity.class).get();
        context.setTheme(R.style.Light);
        mFrameLayout = new FrameLayout(context);
        mFrameLayout.setLayoutParams(new MarginLayoutParams(100, 100));
        mNoContentViewHolder = new NoContentViewHolder(mCardConfiguration, context, mFrameLayout);
        when(mCardConfiguration.getCardBackground()).thenReturn(mRedBackground);
    }

    @Test
    public void testNoContentViewInflated() {
        View view = mFrameLayout.findViewById(R.id.no_content);
        assertThat(view).isNotNull();
        assertThat(view.getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void testBind_setsBackground() {
        mNoContentViewHolder.bind();

        assertThat(mFrameLayout.getBackground()).isEqualTo(mRedBackground);
    }

    @Test
    public void testBind_setsMargins() {
        mNoContentViewHolder.bind();

        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) mNoContentViewHolder.itemView.getLayoutParams();
        assertThat(marginLayoutParams.bottomMargin)
                .isEqualTo(mCardConfiguration.getCardBottomMargin());
        assertThat(marginLayoutParams.leftMargin)
                .isEqualTo(mCardConfiguration.getCardStartMargin());
        assertThat(marginLayoutParams.rightMargin).isEqualTo(mCardConfiguration.getCardEndMargin());
    }
}
