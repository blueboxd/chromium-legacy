// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.viewholders;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.testing.host.stream.FakeCardConfiguration;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link ZeroStateViewHolder}. */
@RunWith(RobolectricTestRunner.class)
public class ZeroStateViewHolderTest {
    @Mock
    private OnClickListener onClickListener;

    private CardConfiguration cardConfiguration;
    private FrameLayout frameLayout;
    private ZeroStateViewHolder zeroStateViewHolder;

    @Before
    public void setup() {
        initMocks(this);
        cardConfiguration = new FakeCardConfiguration();
        Context context = Robolectric.buildActivity(Activity.class).get();
        context.setTheme(R.style.Light);
        frameLayout = new FrameLayout(context);
        frameLayout.setLayoutParams(new MarginLayoutParams(100, 100));
        zeroStateViewHolder = new ZeroStateViewHolder(context, frameLayout, cardConfiguration);
    }

    @Test
    public void testZeroStateViewInflated() {
        View view = frameLayout.findViewById(R.id.zero_state);
        assertThat(view).isNotNull();
        assertThat(view.getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void testBind_setsOnClickListener() {
        zeroStateViewHolder.bind(onClickListener, /* showSpinner= */ false);

        View actionButton = frameLayout.findViewById(R.id.action_button);
        actionButton.performClick();

        verify(onClickListener).onClick(actionButton);
    }

    @Test
    public void testBind_showsLoadingSpinner_whenInitializingFalse() {
        zeroStateViewHolder.bind(onClickListener, /* showSpinner= */ false);

        View spinner = frameLayout.findViewById(R.id.loading_spinner);
        View zeroState = frameLayout.findViewById(R.id.zero_state);

        assertThat(spinner.getVisibility()).isEqualTo(View.GONE);
        assertThat(zeroState.getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void testBind_hidesSpinner() {
        View spinner = frameLayout.findViewById(R.id.loading_spinner);
        View zeroState = frameLayout.findViewById(R.id.zero_state);
        zeroStateViewHolder.showSpinner(true);

        assertThat(spinner.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(zeroState.getVisibility()).isEqualTo(View.GONE);

        zeroStateViewHolder.bind(onClickListener, /* showSpinner= */ false);

        assertThat(spinner.getVisibility()).isEqualTo(View.GONE);
        assertThat(zeroState.getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void testBind_setsMargins() {
        zeroStateViewHolder.bind(onClickListener, /* showSpinner= */ false);

        View zeroStateView = zeroStateViewHolder.itemView.findViewById(R.id.no_content_container);
        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) zeroStateView.getLayoutParams();
        assertThat(marginLayoutParams.bottomMargin)
                .isEqualTo(cardConfiguration.getCardBottomMargin());
        assertThat(marginLayoutParams.leftMargin).isEqualTo(cardConfiguration.getCardStartMargin());
        assertThat(marginLayoutParams.rightMargin).isEqualTo(cardConfiguration.getCardEndMargin());
    }

    @Test
    public void testUnbind() {
        zeroStateViewHolder.bind(onClickListener, /* showSpinner= */ false);

        View actionButton = frameLayout.findViewById(R.id.action_button);
        assertThat(actionButton.hasOnClickListeners()).isTrue();
        assertThat(actionButton.isClickable()).isTrue();

        zeroStateViewHolder.unbind();

        assertThat(actionButton.hasOnClickListeners()).isFalse();
        assertThat(actionButton.isClickable()).isFalse();
    }

    @Test
    public void testShowSpinner_showsSpinner() {
        View spinner = frameLayout.findViewById(R.id.loading_spinner);
        View zeroState = frameLayout.findViewById(R.id.zero_state);

        zeroStateViewHolder.showSpinner(true);

        assertThat(spinner.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(zeroState.getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void testShowSpinner_hidesSpinner() {
        View spinner = frameLayout.findViewById(R.id.loading_spinner);
        View zeroState = frameLayout.findViewById(R.id.zero_state);
        zeroStateViewHolder.showSpinner(true);

        assertThat(spinner.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(zeroState.getVisibility()).isEqualTo(View.GONE);

        zeroStateViewHolder.showSpinner(false);

        assertThat(spinner.getVisibility()).isEqualTo(View.GONE);
        assertThat(zeroState.getVisibility()).isEqualTo(View.VISIBLE);
    }
}
