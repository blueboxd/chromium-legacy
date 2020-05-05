// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.view.View;
import android.widget.ImageView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Tests for {@link BaseSuggestionViewBinder}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BaseSuggestionViewBinderUnitTest {
    @Mock
    BaseSuggestionView mBaseView;

    @Mock
    DecoratedSuggestionView mDecoratedView;

    @Mock
    RoundedCornerImageView mIconView;

    @Mock
    ImageView mActionView;

    @Mock
    ImageView mContentView;

    private Activity mActivity;
    private Resources mResources;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mResources = mActivity.getResources();

        when(mBaseView.getContext()).thenReturn(mActivity);
        when(mIconView.getContext()).thenReturn(mActivity);
        when(mActionView.getContext()).thenReturn(mActivity);
        when(mBaseView.getDecoratedSuggestionView()).thenReturn(mDecoratedView);
        when(mBaseView.getSuggestionImageView()).thenReturn(mIconView);
        when(mBaseView.getActionImageView()).thenReturn(mActionView);
        when(mDecoratedView.getContentView()).thenReturn(mContentView);
        when(mBaseView.getContentView()).thenReturn(mContentView);
        when(mDecoratedView.getResources()).thenReturn(mResources);

        mModel = new PropertyModel(BaseSuggestionViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mBaseView,
                new BaseSuggestionViewBinder(
                        (m, v, p) -> { Assert.assertEquals(mContentView, v); }));
    }

    @Test
    public void decorIcon_showSquareIcon() {
        SuggestionDrawableState state = SuggestionDrawableState.Builder.forColor(0).build();
        mModel.set(BaseSuggestionViewProperties.ICON, state);

        // Expect a single call to setRoundedCorners, and make sure this call sets all radii to 0.
        verify(mIconView).setRoundedCorners(0, 0, 0, 0);
        verify(mIconView).setRoundedCorners(anyInt(), anyInt(), anyInt(), anyInt());

        verify(mIconView).setVisibility(View.VISIBLE);
        verify(mIconView).setImageDrawable(state.drawable);
    }

    @Test
    public void decorIcon_showRoundedIcon() {
        SuggestionDrawableState state =
                SuggestionDrawableState.Builder.forColor(0).setUseRoundedCorners(true).build();
        mModel.set(BaseSuggestionViewProperties.ICON, state);

        // Expect a single call to setRoundedCorners, and make sure this call sets radii to non-0.
        verify(mIconView, never()).setRoundedCorners(0, 0, 0, 0);
        verify(mIconView).setRoundedCorners(anyInt(), anyInt(), anyInt(), anyInt());

        verify(mIconView).setVisibility(View.VISIBLE);
        verify(mIconView).setImageDrawable(state.drawable);
    }

    @Test
    public void decorIcon_hideIcon() {
        InOrder ordered = inOrder(mIconView);

        SuggestionDrawableState state = SuggestionDrawableState.Builder.forColor(0).build();
        mModel.set(BaseSuggestionViewProperties.ICON, state);
        mModel.set(BaseSuggestionViewProperties.ICON, null);

        ordered.verify(mIconView).setVisibility(View.VISIBLE);
        ordered.verify(mIconView).setImageDrawable(state.drawable);
        ordered.verify(mIconView).setVisibility(View.GONE);
        // Ensure we're releasing drawable to free memory.
        ordered.verify(mIconView).setImageDrawable(null);
    }

    @Test
    public void actionIcon_showSquareIcon() {
        SuggestionDrawableState state = SuggestionDrawableState.Builder.forColor(0).build();
        mModel.set(BaseSuggestionViewProperties.ACTION_ICON, state);

        verify(mActionView).setVisibility(View.VISIBLE);
        verify(mActionView).setImageDrawable(state.drawable);
    }

    @Test
    public void actionIcon_hideIcon() {
        InOrder ordered = inOrder(mActionView);

        SuggestionDrawableState state = SuggestionDrawableState.Builder.forColor(0).build();
        mModel.set(BaseSuggestionViewProperties.ACTION_ICON, state);
        mModel.set(BaseSuggestionViewProperties.ACTION_ICON, null);

        ordered.verify(mActionView).setVisibility(View.VISIBLE);
        ordered.verify(mActionView).setImageDrawable(state.drawable);
        ordered.verify(mActionView).setVisibility(View.GONE);
        // Ensure we're releasing drawable to free memory.
        ordered.verify(mActionView).setImageDrawable(null);
    }

    @Test
    public void suggestionPadding_decorIconPresent() {
        final int startSpace = 0;
        final int endSpace = mResources.getDimensionPixelSize(
                R.dimen.omnibox_suggestion_refine_view_modern_end_padding);

        SuggestionDrawableState state = SuggestionDrawableState.Builder.forColor(0).build();
        mModel.set(BaseSuggestionViewProperties.ICON, state);
        verify(mDecoratedView).setPaddingRelative(startSpace, 0, endSpace, 0);
        verify(mBaseView, never()).setPaddingRelative(anyInt(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void suggestionPadding_decorIconAbsent() {
        final int startSpace = mResources.getDimensionPixelSize(
                R.dimen.omnibox_suggestion_start_offset_without_icon);
        final int endSpace = mResources.getDimensionPixelSize(
                R.dimen.omnibox_suggestion_refine_view_modern_end_padding);

        mModel.set(BaseSuggestionViewProperties.ICON, null);
        verify(mDecoratedView).setPaddingRelative(startSpace, 0, endSpace, 0);
        verify(mBaseView, never()).setPaddingRelative(anyInt(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void suggestionDensity_comfortableMode() {
        mModel.set(BaseSuggestionViewProperties.DENSITY,
                BaseSuggestionViewProperties.Density.COMFORTABLE);
        final int expectedPadding =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_comfortable_padding);
        final int expectedHeight =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_comfortable_height);
        verify(mContentView).setPaddingRelative(0, expectedPadding, 0, expectedPadding);
        verify(mContentView).setMinimumHeight(expectedHeight);
    }

    @Test
    public void suggestionDensity_semiCompactMode() {
        mModel.set(BaseSuggestionViewProperties.DENSITY,
                BaseSuggestionViewProperties.Density.SEMICOMPACT);
        final int expectedPadding =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_semicompact_padding);
        final int expectedHeight =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_semicompact_height);
        verify(mContentView).setPaddingRelative(0, expectedPadding, 0, expectedPadding);
        verify(mContentView).setMinimumHeight(expectedHeight);
    }

    @Test
    public void suggestionDensity_compactMode() {
        mModel.set(
                BaseSuggestionViewProperties.DENSITY, BaseSuggestionViewProperties.Density.COMPACT);
        final int expectedPadding =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_compact_padding);
        final int expectedHeight =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_compact_height);
        verify(mContentView).setPaddingRelative(0, expectedPadding, 0, expectedPadding);
        verify(mContentView).setMinimumHeight(expectedHeight);
    }

    @Test
    public void actionCallback_installedWhenSet() {
        ArgumentCaptor<View.OnClickListener> callback =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        Runnable mockCallback = mock(Runnable.class);
        mModel.set(BaseSuggestionViewProperties.ACTION_CALLBACK, mockCallback);
        verify(mActionView, times(1)).setOnClickListener(callback.capture());
        callback.getValue().onClick(null);
        verify(mockCallback, times(1)).run();
    }

    @Test
    public void actionCallback_subsequentUpdatesReplaceCallback() {
        ArgumentCaptor<View.OnClickListener> callback =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        Runnable mockCallback1 = mock(Runnable.class);
        Runnable mockCallback2 = mock(Runnable.class);

        mModel.set(BaseSuggestionViewProperties.ACTION_CALLBACK, mockCallback1);
        mModel.set(BaseSuggestionViewProperties.ACTION_CALLBACK, mockCallback2);
        verify(mActionView, times(2)).setOnClickListener(callback.capture());
        callback.getValue().onClick(null);
        verify(mockCallback1, never()).run();
        verify(mockCallback2, times(1)).run();
    }

    @Test
    public void actionCallback_clearedWhenUnset() {
        mModel.set(BaseSuggestionViewProperties.ACTION_CALLBACK, () -> {});
        verify(mActionView, times(1)).setOnClickListener(any(View.OnClickListener.class));
        mModel.set(BaseSuggestionViewProperties.ACTION_CALLBACK, null);
        verify(mActionView, times(1)).setOnClickListener(null);
    }
}
