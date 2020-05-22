// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;

import android.app.Activity;
import android.support.test.filters.MediumTest;
import android.support.test.rule.ActivityTestRule;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.share.ShareSheetCoordinator.ContentType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivity;

import java.util.Collection;
import java.util.List;
import java.util.Set;

/**
 * Tests {@link ChromeProvidedSharingOptionsProvider}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ChromeProvidedSharingOptionsProviderTest {
    @Rule
    public ActivityTestRule<DummyUiActivity> mActivityTestRule =
            new ActivityTestRule<>(DummyUiActivity.class);

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Mock
    private PrefServiceBridge mPrefServiceBridge;

    private Activity mActivity;
    private ChromeProvidedSharingOptionsProvider mChromeProvidedSharingOptionsProvider;
    private static Set<Integer> sAllContentTypes =
            ImmutableSet.of(ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE,
                    ContentType.TEXT, ContentType.IMAGE, ContentType.OTHER_FILE_TYPE);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = mActivityTestRule.getActivity();
        mChromeProvidedSharingOptionsProvider = new ChromeProvidedSharingOptionsProvider(mActivity,
                /*activityTabProvider=*/null, /*bottomSheetController=*/null,
                new ShareSheetBottomSheetContent(mActivity), mPrefServiceBridge,
                /*shareStartTime=*/0);

        // Return false to indicate printing is disabled.
        Mockito.when(mPrefServiceBridge.getBoolean(anyInt())).thenReturn(false);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(
            {ChromeFeatureList.CHROME_SHARE_SCREENSHOT, ChromeFeatureList.CHROME_SHARE_QRCODE})
    public void
    createPropertyModels_screenshotQrCodeEnabled_includesBoth() {
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.createPropertyModels(sAllContentTypes);

        Assert.assertEquals("Incorrect number of property models.", 4, propertyModels.size());
        assertModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_screenshot),
                        mActivity.getResources().getString(R.string.sharing_copy_url),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label)));
        assertModelsAreFirstParty(propertyModels);
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(
            {ChromeFeatureList.CHROME_SHARE_SCREENSHOT, ChromeFeatureList.CHROME_SHARE_QRCODE})
    public void
    createPropertyModels_screenshotQrCodeDisabled_doesNotIncludeEither() {
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.createPropertyModels(sAllContentTypes);

        Assert.assertEquals("Incorrect number of property models.", 2, propertyModels.size());
        assertModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_url),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title)));
        assertModelsAreFirstParty(propertyModels);
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(
            {ChromeFeatureList.CHROME_SHARE_SCREENSHOT, ChromeFeatureList.CHROME_SHARE_QRCODE})
    public void
    createPropertyModels_printingEnabled_includesPrinting() {
        Mockito.when(mPrefServiceBridge.getBoolean(anyInt())).thenReturn(true);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.createPropertyModels(sAllContentTypes);

        Assert.assertEquals("Incorrect number of property models.", 3, propertyModels.size());
        assertModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_url),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title),
                        mActivity.getResources().getString(R.string.print_share_activity_title)));
        assertModelsAreFirstParty(propertyModels);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(
            {ChromeFeatureList.CHROME_SHARE_SCREENSHOT, ChromeFeatureList.CHROME_SHARE_QRCODE})
    public void
    createPropertyModels_filtersByContentType() {
        Mockito.when(mPrefServiceBridge.getBoolean(anyInt())).thenReturn(true);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.createPropertyModels(
                        ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE));

        Assert.assertEquals("Incorrect number of property models.", 3, propertyModels.size());
        assertModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_url),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label)));
        assertModelsAreFirstParty(propertyModels);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(
            {ChromeFeatureList.CHROME_SHARE_SCREENSHOT, ChromeFeatureList.CHROME_SHARE_QRCODE})
    public void
    createPropertyModels_multipleTypes_filtersByContentType() {
        Mockito.when(mPrefServiceBridge.getBoolean(anyInt())).thenReturn(true);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.createPropertyModels(
                        ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.IMAGE));

        Assert.assertEquals("Incorrect number of property models.", 4, propertyModels.size());
        assertModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_screenshot),
                        mActivity.getResources().getString(R.string.sharing_copy_url),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label)));
        assertModelsAreFirstParty(propertyModels);
    }

    private void assertModelsAreInTheRightOrder(
            List<PropertyModel> propertyModels, List<String> expectedOrder) {
        ImmutableList.Builder<String> actualLabelOrder = ImmutableList.builder();
        for (PropertyModel propertyModel : propertyModels) {
            actualLabelOrder.add(propertyModel.get(ShareSheetItemViewProperties.LABEL));
        }
        assertEquals(
                "Property models in the wrong order.", expectedOrder, actualLabelOrder.build());
    }

    private void assertModelsAreFirstParty(Collection<PropertyModel> propertyModels) {
        for (PropertyModel propertyModel : propertyModels) {
            assertEquals(propertyModel.get(ShareSheetItemViewProperties.LABEL)
                            + " isn't marked as first party.",
                    true, propertyModel.get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
        }
    }
}
