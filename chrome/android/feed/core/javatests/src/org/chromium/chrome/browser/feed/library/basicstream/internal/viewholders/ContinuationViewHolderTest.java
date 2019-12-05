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

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.LoggingListener;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.VisibilityMonitor;
import org.chromium.chrome.browser.feed.library.testing.host.stream.FakeCardConfiguration;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link ContinuationViewHolder}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContinuationViewHolderTest {
    private static final Configuration CONFIGURATION = new Configuration.Builder().build();

    private ContinuationViewHolder mContinuationViewHolder;
    private Context mContext;
    private FrameLayout mFrameLayout;
    private CardConfiguration mCardConfiguration;

    @Mock
    private OnClickListener mOnClickListener;
    @Mock
    private LoggingListener mLoggingListener;
    @Mock
    private VisibilityMonitor mVisibilityMonitor;

    @Before
    public void setup() {
        initMocks(this);
        mCardConfiguration = new FakeCardConfiguration();
        mContext = Robolectric.buildActivity(Activity.class).get();
        mContext.setTheme(R.style.Light);
        mFrameLayout = new FrameLayout(mContext);
        mFrameLayout.setLayoutParams(new MarginLayoutParams(100, 100));
        mContinuationViewHolder = new ContinuationViewHolder(
                CONFIGURATION, mContext, mFrameLayout, mCardConfiguration) {
            @Override
            VisibilityMonitor createVisibilityMonitor(View view, Configuration configuration) {
                return mVisibilityMonitor;
            }
        };
    }

    @Test
    public void testConstructorInflatesView() {
        assertThat((View) mFrameLayout.findViewById(R.id.more_button)).isNotNull();
    }

    @Test
    public void testClick() {
        mContinuationViewHolder.bind(mOnClickListener, mLoggingListener, /* showSpinner= */ false);

        View buttonView = mFrameLayout.findViewById(R.id.action_button);
        buttonView.performClick();

        verify(mOnClickListener).onClick(buttonView);
        verify(mLoggingListener).onContentClicked();
    }

    @Test
    public void testBind_spinnerOff() {
        mContinuationViewHolder.bind(mOnClickListener, mLoggingListener, /* showSpinner= */ false);
        View buttonView = mFrameLayout.findViewById(R.id.action_button);
        View spinnerView = mFrameLayout.findViewById(R.id.loading_spinner);

        assertThat(buttonView.isClickable()).isTrue();
        assertThat(buttonView.getVisibility()).isEqualTo(View.VISIBLE);

        assertThat(spinnerView.getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void testBind_spinnerOn() {
        mContinuationViewHolder.bind(mOnClickListener, mLoggingListener, /* showSpinner= */ true);
        View buttonView = mFrameLayout.findViewById(R.id.action_button);
        View spinnerView = mFrameLayout.findViewById(R.id.loading_spinner);

        assertThat(buttonView.getVisibility()).isEqualTo(View.GONE);
        assertThat(spinnerView.getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void testBind_setsListener() {
        mContinuationViewHolder.bind(mOnClickListener, mLoggingListener, /* showSpinner= */ false);

        verify(mVisibilityMonitor).setListener(mLoggingListener);
    }

    @Test
    public void testBind_setsMargins() {
        mContinuationViewHolder.bind(mOnClickListener, mLoggingListener, /* showSpinner= */ false);

        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) mContinuationViewHolder.itemView.getLayoutParams();
        assertThat(marginLayoutParams.bottomMargin)
                .isEqualTo(mCardConfiguration.getCardBottomMargin());
        assertThat(marginLayoutParams.leftMargin)
                .isEqualTo(mCardConfiguration.getCardStartMargin());
        assertThat(marginLayoutParams.rightMargin).isEqualTo(mCardConfiguration.getCardEndMargin());
        assertThat(marginLayoutParams.topMargin)
                .isEqualTo((int) mContext.getResources().getDimension(
                        R.dimen.feed_more_button_container_top_margins));
    }

    @Test
    public void testUnbind() {
        mContinuationViewHolder.bind(mOnClickListener, mLoggingListener, /* showSpinner= */ false);
        mContinuationViewHolder.unbind();

        View view = mFrameLayout.findViewById(R.id.action_button);
        assertThat(view.isClickable()).isFalse();
        verify(mVisibilityMonitor).setListener(null);
    }
}
