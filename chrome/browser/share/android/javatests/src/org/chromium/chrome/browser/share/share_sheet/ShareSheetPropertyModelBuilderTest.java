// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
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
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetPropertyModelBuilder.ContentType;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivity;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Tests {@link ShareSheetPropertyModelBuilder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB})
public final class ShareSheetPropertyModelBuilderTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public ActivityTestRule<DummyUiActivity> mActivityTestRule =
            new ActivityTestRule<>(DummyUiActivity.class);

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

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

    private static final String IMAGE_TYPE = "image/jpeg";
    private static final String URL = "http://www.google.com/";

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
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15Enabled_hasCorrectLinkContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", URL).build();

        assertEquals("Should contain LINK_PAGE_NOT_VISIBLE.",
                ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, /*isUrlOfPageVisible=*/
                        false));
        assertEquals("Should contain LINK_PAGE_VISIBLE.",
                ImmutableSet.of(ContentType.LINK_PAGE_VISIBLE),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, /*isUrlOfPageVisible=*/
                        true));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15EnabledAndNoUrl_hasNoLinkContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", "").build();

        assertEquals("Should not contain LINK_PAGE_NOT_VISIBLE", ImmutableSet.of(),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, /*isUrlOfPageVisible=*/
                        false));
        assertEquals("Should not contain LINK_PAGE_VISIBLE.", ImmutableSet.of(),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, /*isUrlOfPageVisible=*/
                        true));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15EnabledAndUrlDifferentFromText_hasTextContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", "").setText("testText").build();

        assertEquals("Should contain TEXT.", ImmutableSet.of(ContentType.TEXT),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, /*isUrlOfPageVisible=*/
                        false));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15EnabledAndTextIsNull_hasNoTextContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", "").build();

        assertEquals("Should not contain TEXT.", ImmutableSet.of(),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, /*isUrlOfPageVisible=*/
                        false));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15Enabled_hasImageContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", "")
                                          .setFileUris(new ArrayList<>(ImmutableSet.of(Uri.EMPTY)))
                                          .setFileContentType(IMAGE_TYPE)
                                          .build();

        assertEquals("Should contain IMAGE.", ImmutableSet.of(ContentType.IMAGE),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, /*isUrlOfPageVisible=*/
                        false));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15EnabledAndNoFiles_hasNoImageContentType() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", "").setFileContentType(IMAGE_TYPE).build();

        assertEquals("Should not contain IMAGE.", ImmutableSet.of(),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, /*isUrlOfPageVisible=*/
                        false));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15Enabled_hasOtherFileContentType() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", "")
                        .setFileUris(new ArrayList<>(ImmutableList.of(Uri.EMPTY, Uri.EMPTY)))
                        .setFileContentType("*/*")
                        .build();

        assertEquals("Should contain OTHER_FILE_TYPE.",
                ImmutableSet.of(ContentType.OTHER_FILE_TYPE),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, /*isUrlOfPageVisible=*/
                        false));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15EnabledAndNoFiles_hasNoFileContentType() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", "").setFileContentType("*/*").build();

        assertEquals("Should not contain OTHER_FILE_TYPE.", ImmutableSet.of(),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, /*isUrlOfPageVisible=*/
                        false));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15Enabled_hasMultipleContentTypes() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", URL)
                        .setText("testText")
                        .setFileUris(new ArrayList<>(ImmutableList.of(Uri.EMPTY, Uri.EMPTY)))
                        .setFileContentType("*/*")
                        .build();

        assertEquals("Should contain correct content types.",
                ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.OTHER_FILE_TYPE,
                        ContentType.TEXT),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, /*isUrlOfPageVisible=*/
                        false));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15Disabled_returnsAllContentTypes() {
        assertEquals("Should contain all content types.",
                ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES,
                ShareSheetPropertyModelBuilder.getContentTypes(null, false));
    }

    @Test
    @MediumTest
    public void testSelectThirdPartyApps() {
        ShareSheetPropertyModelBuilder builder =
                new ShareSheetPropertyModelBuilder(null, mPackageManager);

        ArrayList<PropertyModel> propertyModels = builder.selectThirdPartyApps(
                null, mParams, /*saveLastUsed=*/false, /*shareStartTime=*/0);
        Assert.assertEquals("Incorrect number of property models.", 3, propertyModels.size());
        Assert.assertEquals("First property model isn't testModel3", "testModel3",
                propertyModels.get(0).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("Second property model isn't testModel1", "testModel1",
                propertyModels.get(1).get(ShareSheetItemViewProperties.LABEL));
        Assert.assertEquals("Third property model isn't testModel2", "testModel2",
                propertyModels.get(2).get(ShareSheetItemViewProperties.LABEL));
    }
}
