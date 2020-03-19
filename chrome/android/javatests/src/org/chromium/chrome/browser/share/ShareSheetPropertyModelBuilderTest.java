// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.support.test.filters.MediumTest;
import android.support.test.rule.ActivityTestRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;

/** Tests {@link ShareSheetPropertyModelBuilder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class ShareSheetPropertyModelBuilderTest {
    @Rule
    public ActivityTestRule<DummyUiActivity> mActivityTestRule =
            new ActivityTestRule<>(DummyUiActivity.class);

    @Mock
    private PackageManager mPackageManager;

    @Mock
    private ShareParams mParams;

    @Mock
    private ResolveInfo mResolveInfo1;

    @Mock
    private ResolveInfo mResolveInfo2;

    @Mock
    private ResolveInfo mResolveInfo3;

    private ArrayList<ResolveInfo> mResolveInfoList;
    private Activity mActivity;

    @Before
    public void setUp() throws PackageManager.NameNotFoundException {
        MockitoAnnotations.initMocks(this);
        mActivity = mActivityTestRule.getActivity();

        mResolveInfo1.activityInfo =
                mActivity.getPackageManager().getActivityInfo(mActivity.getComponentName(), 0);
        mResolveInfo1.activityInfo.packageName = "testPackage1";
        when(mResolveInfo1.loadLabel(any())).thenReturn("testModel1");
        when(mResolveInfo1.loadIcon(any())).thenReturn(null);
        mResolveInfo1.icon = 0;

        mResolveInfo2.activityInfo =
                mActivity.getPackageManager().getActivityInfo(mActivity.getComponentName(), 0);
        mResolveInfo2.activityInfo.packageName = "testPackage2";
        when(mResolveInfo2.loadLabel(any())).thenReturn("testModel2");
        when(mResolveInfo2.loadIcon(any())).thenReturn(null);
        mResolveInfo2.icon = 0;

        mResolveInfo3.activityInfo =
                mActivity.getPackageManager().getActivityInfo(mActivity.getComponentName(), 0);
        mResolveInfo3.activityInfo.packageName = "testPackage3";
        mResolveInfo3.activityInfo.name = "com.google.android.gm.ComposeActivityGmailExternal";
        when(mResolveInfo3.loadLabel(any())).thenReturn("testModel3");
        when(mResolveInfo3.loadIcon(any())).thenReturn(null);
        mResolveInfo3.icon = 0;

        mResolveInfoList =
                new ArrayList<>(Arrays.asList(mResolveInfo1, mResolveInfo2, mResolveInfo3));
        when(mPackageManager.queryIntentActivities(any(), anyInt())).thenReturn(mResolveInfoList);
        when(mPackageManager.getResourcesForApplication(anyString()))
                .thenReturn(mActivity.getResources());

        // Explicitly add the test feature so the call to getFieldTrialParamByFeature returns an
        // empty string.
        ChromeFeatureList.setTestFeatures(
                Collections.singletonMap(ChromeFeatureList.CHROME_SHARING_HUB, true));
    }

    @Test
    @MediumTest
    public void testSelectThirdPartyApps() {
        ShareSheetPropertyModelBuilder builder =
                new ShareSheetPropertyModelBuilder(null, mPackageManager);

        ArrayList<PropertyModel> propertyModels = builder.selectThirdPartyApps(null, mParams);
        Assert.assertEquals("Incorrect number of property models.", 3, propertyModels.size());
        Assert.assertEquals("First property model isn't testModel3", "testModel3",
                propertyModels.get(0).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("Second property model isn't testModel1", "testModel1",
                propertyModels.get(1).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("Third property model isn't testModel2", "testModel2",
                propertyModels.get(2).get(ShareSheetItemViewProperties.LABEL));
    }
}
