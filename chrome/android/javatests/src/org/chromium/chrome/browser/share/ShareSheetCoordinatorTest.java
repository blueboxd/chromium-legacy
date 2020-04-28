// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyBoolean;
import static org.mockito.Matchers.anyInt;

import android.app.Activity;
import android.support.test.filters.MediumTest;
import android.support.test.rule.ActivityTestRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivity;

import java.util.ArrayList;
import java.util.Arrays;

/** Tests {@link ShareSheetCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class ShareSheetCoordinatorTest {
    @Rule
    public ActivityTestRule<DummyUiActivity> mActivityTestRule =
            new ActivityTestRule<>(DummyUiActivity.class);

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Mock
    private ShareSheetPropertyModelBuilder mPropertyModelBuilder;

    @Mock
    private PrefServiceBridge mPrefServiceBridge;

    private ArrayList<PropertyModel> mThirdPartyPropertyModels;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PropertyModel testModel1 = new PropertyModel.Builder(ShareSheetItemViewProperties.ALL_KEYS)
                                           .with(ShareSheetItemViewProperties.ICON, null)
                                           .with(ShareSheetItemViewProperties.LABEL, "testModel1")
                                           .with(ShareSheetItemViewProperties.CLICK_LISTENER, null)
                                           .with(ShareSheetItemViewProperties.IS_FIRST_PARTY, false)
                                           .build();
        PropertyModel testModel2 = new PropertyModel.Builder(ShareSheetItemViewProperties.ALL_KEYS)
                                           .with(ShareSheetItemViewProperties.ICON, null)
                                           .with(ShareSheetItemViewProperties.LABEL, "testModel2")
                                           .with(ShareSheetItemViewProperties.CLICK_LISTENER, null)
                                           .with(ShareSheetItemViewProperties.IS_FIRST_PARTY, false)
                                           .build();

        mThirdPartyPropertyModels =
                new ArrayList<PropertyModel>(Arrays.asList(testModel1, testModel2));
        Mockito.when(mPropertyModelBuilder.selectThirdPartyApps(any(), any()))
                .thenReturn(mThirdPartyPropertyModels);
        Mockito.when(mPropertyModelBuilder.createPropertyModel(any(), any(), any(), anyBoolean()))
                .thenCallRealMethod();
        // Return true to indicate printing is enabled.
        Mockito.when(mPrefServiceBridge.getBoolean(anyInt())).thenReturn(true);
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARE_SCREENSHOT})
    public void testCreateTopRowPropertyModelsScreenshotsDisabled() {
        ShareSheetCoordinator coordinator =
                new ShareSheetCoordinator(null, null, mPropertyModelBuilder, mPrefServiceBridge);
        Activity activity = mActivityTestRule.getActivity();

        ShareSheetBottomSheetContent bottomSheet = new ShareSheetBottomSheetContent(activity);

        ArrayList<PropertyModel> propertyModels =
                coordinator.createTopRowPropertyModels(bottomSheet, activity);
        Assert.assertEquals("Incorrect number of property models.", 4, propertyModels.size());
        Assert.assertEquals("First property model isn't Copy URL.",
                activity.getResources().getString(R.string.sharing_copy_url),
                propertyModels.get(0).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("First property model isn't marked as first party.", true,
                propertyModels.get(0).get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
        Assert.assertEquals("Second property model isn't SendTabToSelf.",
                activity.getResources().getString(R.string.send_tab_to_self_share_activity_title),
                propertyModels.get(1).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("Second property model isn't marked as first party.", true,
                propertyModels.get(1).get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
        Assert.assertEquals("Third property model isn't QR Code.",
                activity.getResources().getString(R.string.qr_code_share_icon_label),
                propertyModels.get(2).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("Third property model isn't marked as first party.", true,
                propertyModels.get(2).get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
        Assert.assertEquals("Fourth property model isn't Print.",
                activity.getResources().getString(R.string.print_share_activity_title),
                propertyModels.get(3).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("Fourth property model isn't marked as first party.", true,
                propertyModels.get(3).get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARE_SCREENSHOT})
    public void testCreateTopRowPropertyModelsScreenshotsEnabled() {
        ShareSheetCoordinator coordinator =
                new ShareSheetCoordinator(null, null, mPropertyModelBuilder, mPrefServiceBridge);
        Activity activity = mActivityTestRule.getActivity();
        ShareSheetBottomSheetContent bottomSheet = new ShareSheetBottomSheetContent(activity);

        ArrayList<PropertyModel> propertyModels =
                coordinator.createTopRowPropertyModels(bottomSheet, activity);
        Assert.assertEquals("Incorrect number of property models.", 5, propertyModels.size());
        Assert.assertEquals("First property model isn't Screenshotz.",
                activity.getResources().getString(R.string.sharing_screenshot),
                propertyModels.get(0).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("First property model isn't marked as first party.", true,
                propertyModels.get(0).get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
        Assert.assertEquals("Second property model isn't Copy URL.",
                activity.getResources().getString(R.string.sharing_copy_url),
                propertyModels.get(1).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("Second property model isn't marked as first party.", true,
                propertyModels.get(1).get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
        Assert.assertEquals("Third property model isn't SendTabToSelf.",
                activity.getResources().getString(R.string.send_tab_to_self_share_activity_title),
                propertyModels.get(2).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("Third property model isn't marked as first party.", true,
                propertyModels.get(2).get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
        Assert.assertEquals("Fourth property model isn't QR Code.",
                activity.getResources().getString(R.string.qr_code_share_icon_label),
                propertyModels.get(3).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("Fourth property model isn't marked as first party.", true,
                propertyModels.get(3).get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
        Assert.assertEquals("Fifth property model isn't Print.",
                activity.getResources().getString(R.string.print_share_activity_title),
                propertyModels.get(4).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("Fifth property model isn't marked as first party.", true,
                propertyModels.get(4).get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
    }

    @Test
    @MediumTest
    public void testCreateBottomRowPropertyModels() {
        ShareSheetCoordinator coordinator =
                new ShareSheetCoordinator(null, null, mPropertyModelBuilder, mPrefServiceBridge);
        Activity activity = mActivityTestRule.getActivity();
        ShareSheetBottomSheetContent bottomSheet = new ShareSheetBottomSheetContent(activity);

        ArrayList<PropertyModel> propertyModels = coordinator.createBottomRowPropertyModels(
                bottomSheet, activity, /*shareParams=*/null);
        Assert.assertEquals("Incorrect number of property models.", 3, propertyModels.size());
        Assert.assertEquals("First property model isn't testModel1.", "testModel1",
                propertyModels.get(0).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("First property model is marked as first party.", false,
                propertyModels.get(0).get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
        Assert.assertEquals("Second property model isn't testModel2.", "testModel2",
                propertyModels.get(1).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("Second property model is marked as first party.", false,
                propertyModels.get(1).get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
        Assert.assertEquals("Third property model isn't More.",
                activity.getResources().getString(R.string.sharing_more_icon_label),
                propertyModels.get(2).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("Thired property model isn't marked as first party.", true,
                propertyModels.get(2).get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
    }
}
