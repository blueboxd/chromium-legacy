// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyString;

import android.app.Activity;
import android.support.test.rule.ActivityTestRule;

import androidx.test.filters.MediumTest;

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
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetPropertyModelBuilder.ContentType;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivity;

import java.util.Collection;
import java.util.List;

/**
 * Tests {@link ChromeProvidedSharingOptionsProvider}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ChromeProvidedSharingOptionsProviderTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public ActivityTestRule<DummyUiActivity> mActivityTestRule =
            new ActivityTestRule<>(DummyUiActivity.class);

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Mock
    private PrefServiceBridge mPrefServiceBridge;

    private static final String URL = "http://www.google.com/";

    @Mock
    private ShareSheetCoordinator mShareSheetCoordinator;

    private Activity mActivity;
    private ChromeProvidedSharingOptionsProvider mChromeProvidedSharingOptionsProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = mActivityTestRule.getActivity();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(
            {ChromeFeatureList.CHROME_SHARE_SCREENSHOT, ChromeFeatureList.CHROME_SHARE_QRCODE})
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void
    createPropertyModels_screenshotQrCodeEnabled_includesBoth() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/false);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES);

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
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARE_SCREENSHOT,
            ChromeFeatureList.CHROME_SHARE_QRCODE, ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void
    createPropertyModels_screenshotQrCodeDisabled_doesNotIncludeEither() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/false);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES);

        Assert.assertEquals("Incorrect number of property models.", 2, propertyModels.size());
        assertModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_url),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title)));
        assertModelsAreFirstParty(propertyModels);
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARE_SCREENSHOT,
            ChromeFeatureList.CHROME_SHARE_QRCODE, ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void
    createPropertyModels_printingEnabled_includesPrinting() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/true);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES);

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
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    @Features.DisableFeatures(
            {ChromeFeatureList.CHROME_SHARE_SCREENSHOT, ChromeFeatureList.CHROME_SHARE_QRCODE,
                    ChromeFeatureList.CHROME_SHARE_HIGHLIGHTS_ANDROID})
    public void
    createPropertyModels_sharingHub15Enabled_includesCopyText() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/false);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.TEXT));

        Assert.assertEquals("Incorrect number of property models.", 1, propertyModels.size());
        assertModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_text)));
        assertModelsAreFirstParty(propertyModels);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(
            {ChromeFeatureList.CHROME_SHARE_SCREENSHOT, ChromeFeatureList.CHROME_SHARE_QRCODE})
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void
    createPropertyModels_filtersByContentType() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/true);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
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
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void
    createPropertyModels_multipleTypes_filtersByContentType() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/true);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
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

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARE_HIGHLIGHTS_ANDROID})
    public void createPropertyModels_sharingHub15Disabled_noHighlights() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/false);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.TEXT));

        Assert.assertEquals("Incorrect number of property models.", 0, propertyModels.size());
        assertModelsAreFirstParty(propertyModels);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15,
            ChromeFeatureList.CHROME_SHARE_HIGHLIGHTS_ANDROID})
    public void
    createPropertyModels_sharingHub15HighlightsEnabled() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/false);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.TEXT));

        Assert.assertEquals("Incorrect number of property models.", 2, propertyModels.size());
        assertModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_text),
                        mActivity.getResources().getString(R.string.sharing_highlights)));
        assertModelsAreFirstParty(propertyModels);
    }

    @Test
    @MediumTest
    public void getUrlToShare_noShareParamsUrl_returnsImageUrl() {
        ShareParams shareParams = new ShareParams.Builder(null, /*title=*/"", /*url=*/"").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setImageSrcUrl(URL).build();

        assertEquals("URL should be imageSrcUrl.",
                ChromeProvidedSharingOptionsProvider.getUrlToShare(shareParams, chromeShareExtras),
                URL);
    }

    @Test
    @MediumTest
    public void getUrlToShare_shareParamsUrlExists_returnsShareParamsUrl() {
        ShareParams shareParams = new ShareParams.Builder(null, /*title=*/"", URL).build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setImageSrcUrl("").build();

        assertEquals("URL should be ShareParams URL.",
                ChromeProvidedSharingOptionsProvider.getUrlToShare(shareParams, chromeShareExtras),
                URL);
    }

    private void setUpChromeProvidedSharingOptionsProviderTest(boolean printingEnabled) {
        Mockito.when(mPrefServiceBridge.getBoolean(anyString())).thenReturn(printingEnabled);

        mChromeProvidedSharingOptionsProvider = new ChromeProvidedSharingOptionsProvider(mActivity,
                /*activityTabProvider=*/null, /*bottomSheetController=*/null,
                new ShareSheetBottomSheetContent(mActivity, mShareSheetCoordinator),
                mPrefServiceBridge, new ShareParams.Builder(null, "", "").build(),
                new ChromeShareExtras.Builder().build(),
                /*TabPrinterDelegate=*/null,
                /*shareStartTime=*/0, mShareSheetCoordinator);
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
