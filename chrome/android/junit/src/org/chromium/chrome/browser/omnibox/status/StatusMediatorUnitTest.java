// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.res.Resources;
import android.graphics.Bitmap;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Unit tests for {@link StatusMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class StatusMediatorUnitTest {
    private static final String TEST_SEARCH_URL = "https://www.test.com";

    @Mock
    Resources mResources;
    @Mock
    StatusMediator.StatusMediatorDelegate mDelegate;
    @Mock
    Bitmap mBitmap;
    @Captor
    ArgumentCaptor<Callback<Bitmap>> mCallbackCaptor;

    PropertyModel mModel;
    StatusMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mModel = new PropertyModel(StatusProperties.ALL_KEYS);
        mMediator = new StatusMediator(mModel, mResources);
        mMediator.setDelegateForTesting(mDelegate);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_showGoogleLogo() {
        setupSearchEngineLogoForTesting(false, true, false);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.updateSearchEngineStatusIcon(true, true, TEST_SEARCH_URL);
        Assert.assertEquals(
                R.drawable.ic_logo_googleg_24dp, mModel.get(StatusProperties.STATUS_ICON_RES));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_showGoogleLogo_searchLoupeEverywhere() {
        setupSearchEngineLogoForTesting(false, true, true);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.updateSearchEngineStatusIcon(true, true, TEST_SEARCH_URL);
        Assert.assertEquals(R.drawable.ic_search, mModel.get(StatusProperties.STATUS_ICON_RES));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_showNonGoogleLogo() {
        setupSearchEngineLogoForTesting(false, false, false);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);

        // Clear invocations since the setup methods call updateLocationBarIcon.
        Mockito.clearInvocations(mDelegate);
        mMediator.updateSearchEngineStatusIcon(true, false, TEST_SEARCH_URL);
        Assert.assertEquals(R.drawable.ic_search, mModel.get(StatusProperties.STATUS_ICON_RES));
        Mockito.verify(mDelegate, Mockito.times(1))
                .getSearchEngineLogoFavicon(Mockito.any(), Mockito.any());
        mCallbackCaptor.getValue().onResult(mBitmap);
        Assert.assertEquals(mBitmap, mModel.get(StatusProperties.STATUS_ICON));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_showNonGoogleLogo_searchLoupeEverywhere() {
        setupSearchEngineLogoForTesting(false, false, true);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);

        // Clear invocations since the setup methods call updateLocationBarIcon.
        Mockito.clearInvocations(mDelegate);
        mMediator.updateSearchEngineStatusIcon(true, false, TEST_SEARCH_URL);
        Assert.assertEquals(R.drawable.ic_search, mModel.get(StatusProperties.STATUS_ICON_RES));
        Mockito.verify(mDelegate, Mockito.times(0))
                .getSearchEngineLogoFavicon(Mockito.any(), Mockito.any());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_onTextChanged_globeReplacesIconWhenTextIsSite() {
        setupSearchEngineLogoForTesting(true, false, false);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.onTextChanged("", "testing");
        Assert.assertEquals(R.drawable.ic_globe_24dp, mModel.get(StatusProperties.STATUS_ICON_RES));
    }

    private void setupSearchEngineLogoForTesting(
            boolean validUrl, boolean showGoogle, boolean loupeEverywhere) {
        Mockito.doReturn(validUrl).when(mDelegate).isUrlValid(Mockito.any());
        Mockito.doReturn(true).when(mDelegate).shouldShowSearchEngineLogo();
        Mockito.doReturn(loupeEverywhere).when(mDelegate).shouldShowSearchLoupeEverywhere();
        Mockito.doNothing().when(mDelegate).getSearchEngineLogoFavicon(
                Mockito.any(), mCallbackCaptor.capture());
        mMediator.updateSearchEngineStatusIcon(true, showGoogle, TEST_SEARCH_URL);
    }
}
