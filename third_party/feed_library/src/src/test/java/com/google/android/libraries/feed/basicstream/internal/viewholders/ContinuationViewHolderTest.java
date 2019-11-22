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

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.sharedstream.logging.LoggingListener;
import com.google.android.libraries.feed.sharedstream.logging.VisibilityMonitor;
import com.google.android.libraries.feed.testing.host.stream.FakeCardConfiguration;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link ContinuationViewHolder}. */
@RunWith(RobolectricTestRunner.class)
public class ContinuationViewHolderTest {
    private static final Configuration CONFIGURATION = new Configuration.Builder().build();

    private ContinuationViewHolder continuationViewHolder;
    private Context context;
    private FrameLayout frameLayout;
    private CardConfiguration cardConfiguration;

    @Mock
    private OnClickListener onClickListener;
    @Mock
    private LoggingListener loggingListener;
    @Mock
    private VisibilityMonitor visibilityMonitor;

    @Before
    public void setup() {
        initMocks(this);
        cardConfiguration = new FakeCardConfiguration();
        context = Robolectric.buildActivity(Activity.class).get();
        context.setTheme(R.style.Light);
        frameLayout = new FrameLayout(context);
        frameLayout.setLayoutParams(new MarginLayoutParams(100, 100));
        continuationViewHolder = new ContinuationViewHolder(
                CONFIGURATION, context, frameLayout, cardConfiguration) {
            @Override
            VisibilityMonitor createVisibilityMonitor(View view, Configuration configuration) {
                return visibilityMonitor;
            }
        };
    }

    @Test
    public void testConstructorInflatesView() {
        assertThat((View) frameLayout.findViewById(R.id.more_button)).isNotNull();
    }

    @Test
    public void testClick() {
        continuationViewHolder.bind(onClickListener, loggingListener, /* showSpinner= */ false);

        View buttonView = frameLayout.findViewById(R.id.action_button);
        buttonView.performClick();

        verify(onClickListener).onClick(buttonView);
        verify(loggingListener).onContentClicked();
    }

    @Test
    public void testBind_spinnerOff() {
        continuationViewHolder.bind(onClickListener, loggingListener, /* showSpinner= */ false);
        View buttonView = frameLayout.findViewById(R.id.action_button);
        View spinnerView = frameLayout.findViewById(R.id.loading_spinner);

        assertThat(buttonView.isClickable()).isTrue();
        assertThat(buttonView.getVisibility()).isEqualTo(View.VISIBLE);

        assertThat(spinnerView.getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void testBind_spinnerOn() {
        continuationViewHolder.bind(onClickListener, loggingListener, /* showSpinner= */ true);
        View buttonView = frameLayout.findViewById(R.id.action_button);
        View spinnerView = frameLayout.findViewById(R.id.loading_spinner);

        assertThat(buttonView.getVisibility()).isEqualTo(View.GONE);
        assertThat(spinnerView.getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void testBind_setsListener() {
        continuationViewHolder.bind(onClickListener, loggingListener, /* showSpinner= */ false);

        verify(visibilityMonitor).setListener(loggingListener);
    }

    @Test
    public void testBind_setsMargins() {
        continuationViewHolder.bind(onClickListener, loggingListener, /* showSpinner= */ false);

        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) continuationViewHolder.itemView.getLayoutParams();
        assertThat(marginLayoutParams.bottomMargin)
                .isEqualTo(cardConfiguration.getCardBottomMargin());
        assertThat(marginLayoutParams.leftMargin).isEqualTo(cardConfiguration.getCardStartMargin());
        assertThat(marginLayoutParams.rightMargin).isEqualTo(cardConfiguration.getCardEndMargin());
        assertThat(marginLayoutParams.topMargin)
                .isEqualTo((int) context.getResources().getDimension(
                        R.dimen.feed_more_button_container_top_margins));
    }

    @Test
    public void testUnbind() {
        continuationViewHolder.bind(onClickListener, loggingListener, /* showSpinner= */ false);
        continuationViewHolder.unbind();

        View view = frameLayout.findViewById(R.id.action_button);
        assertThat(view.isClickable()).isFalse();
        verify(visibilityMonitor).setListener(null);
    }
}
