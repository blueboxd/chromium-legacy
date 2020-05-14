// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.chrome.R;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Tests for {@link BaseSuggestionViewProcessor}.
 */
@RunWith(LocalRobolectricTestRunner.class)
public class HeaderViewBinderUnitTest {
    Activity mActivity;
    PropertyModel mModel;

    @Mock
    HeaderView mHeaderView;
    @Mock
    TextView mHeaderText;
    @Mock
    ImageView mHeaderIcon;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Light);

        when(mHeaderView.getContext()).thenReturn(mActivity);
        when(mHeaderView.getResources()).thenReturn(mActivity.getResources());
        when(mHeaderView.getTextView()).thenReturn(mHeaderText);
        when(mHeaderView.getIconView()).thenReturn(mHeaderIcon);

        mModel = new PropertyModel(HeaderViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mHeaderView, HeaderViewBinder::bind);
    }

    @Test
    public void actionIcon_iconReflectsExpandedState() {
        // Expand.
        mModel.set(HeaderViewProperties.IS_EXPANDED, true);
        verify(mHeaderIcon, times(1)).setImageResource(R.drawable.ic_expand_less_black_24dp);

        // Collapse.
        mModel.set(HeaderViewProperties.IS_EXPANDED, false);
        verify(mHeaderIcon, times(1)).setImageResource(R.drawable.ic_expand_more_black_24dp);
    }

    @Test
    public void headerView_accessibilityStringReflectsExpandedState() {
        // Expand without title.
        mModel.set(HeaderViewProperties.IS_EXPANDED, true);
        verify(mHeaderView, times(1)).setExpandedStateForAccessibility(true);

        mModel.set(HeaderViewProperties.IS_EXPANDED, false);
        verify(mHeaderView, times(1)).setExpandedStateForAccessibility(false);
    }

    @Test
    public void headerView_listenerInstalledWhenDelegateIsSet() {
        final Runnable callback = mock(Runnable.class);
        final ArgumentCaptor<View.OnClickListener> captor =
                ArgumentCaptor.forClass(View.OnClickListener.class);

        // Install.
        mModel.set(HeaderViewProperties.DELEGATE, callback);
        verify(mHeaderView, times(1)).setOnClickListener(captor.capture());

        // Call.
        captor.getValue().onClick(null);
        verify(callback, times(1)).run();

        // Remove.
        mModel.set(HeaderViewProperties.DELEGATE, null);
        verify(mHeaderView, times(1)).setOnClickListener(null);
    }
}
