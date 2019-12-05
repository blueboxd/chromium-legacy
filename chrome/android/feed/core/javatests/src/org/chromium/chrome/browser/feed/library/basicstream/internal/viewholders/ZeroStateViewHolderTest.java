// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.testing.host.stream.FakeCardConfiguration;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link ZeroStateViewHolder}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ZeroStateViewHolderTest {
    @Mock
    private OnClickListener mOnClickListener;

    private CardConfiguration mCardConfiguration;
    private FrameLayout mFrameLayout;
    private ZeroStateViewHolder mZeroStateViewHolder;

    @Before
    public void setup() {
        initMocks(this);
        mCardConfiguration = new FakeCardConfiguration();
        Context context = Robolectric.buildActivity(Activity.class).get();
        context.setTheme(R.style.Light);
        mFrameLayout = new FrameLayout(context);
        mFrameLayout.setLayoutParams(new MarginLayoutParams(100, 100));
        mZeroStateViewHolder = new ZeroStateViewHolder(context, mFrameLayout, mCardConfiguration);
    }

    @Test
    public void testZeroStateViewInflated() {
        View view = mFrameLayout.findViewById(R.id.zero_state);
        assertThat(view).isNotNull();
        assertThat(view.getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void testBind_setsOnClickListener() {
        mZeroStateViewHolder.bind(mOnClickListener, /* showSpinner= */ false);

        View actionButton = mFrameLayout.findViewById(R.id.action_button);
        actionButton.performClick();

        verify(mOnClickListener).onClick(actionButton);
    }

    @Test
    public void testBind_showsLoadingSpinner_whenInitializingFalse() {
        mZeroStateViewHolder.bind(mOnClickListener, /* showSpinner= */ false);

        View spinner = mFrameLayout.findViewById(R.id.loading_spinner);
        View zeroState = mFrameLayout.findViewById(R.id.zero_state);

        assertThat(spinner.getVisibility()).isEqualTo(View.GONE);
        assertThat(zeroState.getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void testBind_hidesSpinner() {
        View spinner = mFrameLayout.findViewById(R.id.loading_spinner);
        View zeroState = mFrameLayout.findViewById(R.id.zero_state);
        mZeroStateViewHolder.showSpinner(true);

        assertThat(spinner.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(zeroState.getVisibility()).isEqualTo(View.GONE);

        mZeroStateViewHolder.bind(mOnClickListener, /* showSpinner= */ false);

        assertThat(spinner.getVisibility()).isEqualTo(View.GONE);
        assertThat(zeroState.getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void testBind_setsMargins() {
        mZeroStateViewHolder.bind(mOnClickListener, /* showSpinner= */ false);

        View zeroStateView = mZeroStateViewHolder.itemView.findViewById(R.id.no_content_container);
        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) zeroStateView.getLayoutParams();
        assertThat(marginLayoutParams.bottomMargin)
                .isEqualTo(mCardConfiguration.getCardBottomMargin());
        assertThat(marginLayoutParams.leftMargin)
                .isEqualTo(mCardConfiguration.getCardStartMargin());
        assertThat(marginLayoutParams.rightMargin).isEqualTo(mCardConfiguration.getCardEndMargin());
    }

    @Test
    public void testUnbind() {
        mZeroStateViewHolder.bind(mOnClickListener, /* showSpinner= */ false);

        View actionButton = mFrameLayout.findViewById(R.id.action_button);
        assertThat(actionButton.hasOnClickListeners()).isTrue();
        assertThat(actionButton.isClickable()).isTrue();

        mZeroStateViewHolder.unbind();

        assertThat(actionButton.hasOnClickListeners()).isFalse();
        assertThat(actionButton.isClickable()).isFalse();
    }

    @Test
    public void testShowSpinner_showsSpinner() {
        View spinner = mFrameLayout.findViewById(R.id.loading_spinner);
        View zeroState = mFrameLayout.findViewById(R.id.zero_state);

        mZeroStateViewHolder.showSpinner(true);

        assertThat(spinner.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(zeroState.getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void testShowSpinner_hidesSpinner() {
        View spinner = mFrameLayout.findViewById(R.id.loading_spinner);
        View zeroState = mFrameLayout.findViewById(R.id.zero_state);
        mZeroStateViewHolder.showSpinner(true);

        assertThat(spinner.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(zeroState.getVisibility()).isEqualTo(View.GONE);

        mZeroStateViewHolder.showSpinner(false);

        assertThat(spinner.getVisibility()).isEqualTo(View.GONE);
        assertThat(zeroState.getVisibility()).isEqualTo(View.VISIBLE);
    }
}
