// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import android.support.test.filters.MediumTest;

import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.download.home.filter.FilterCoordinator;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.download.ui.StubbedProvider;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.UiRestriction;

import java.util.HashMap;
import java.util.Map;

/** Tests the DownloadActivity home V2. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class DownloadActivityV2Test extends DummyUiActivityTestCase {
    @Mock
    private Profile mProfile;
    @Mock
    private Tracker mTracker;
    @Mock
    private SnackbarManager mSnackbarManager;
    @Mock
    private ModalDialogManager.Presenter mAppModalPresenter;

    private ModalDialogManager mModalDialogManager;

    private DownloadManagerCoordinator mDownloadCoordinator;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        UiUtils.setDisableUrlFormattingForTests(true);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE, true);
        features.put(ChromeFeatureList.DOWNLOAD_HOME_SHOW_STORAGE_INFO, true);
        features.put(ChromeFeatureList.DOWNLOAD_HOME_V2, true);
        features.put(ChromeFeatureList.OFFLINE_PAGES_PREFETCHING, true);
        features.put(ChromeFeatureList.OVERSCROLL_HISTORY_NAVIGATION, false);
        features.put(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER, false);
        features.put(ChromeFeatureList.DOWNLOAD_RENAME, true);
        ChromeFeatureList.setTestFeatures(features);

        StubbedOfflineContentProvider stubbedOfflineContentProvider =
                new StubbedOfflineContentProvider();
        OfflineContentAggregatorFactory.setOfflineContentProviderForTests(
                stubbedOfflineContentProvider);

        OfflineItem item0 = StubbedProvider.createOfflineItem(0, "20151019 07:26");
        OfflineItem item1 = StubbedProvider.createOfflineItem(1, "20151020 07:27");
        OfflineItem item2 = StubbedProvider.createOfflineItem(2, "20151021 07:28");
        OfflineItem item3 = StubbedProvider.createOfflineItem(3, "20151021 07:29");

        stubbedOfflineContentProvider.addItem(item0);
        stubbedOfflineContentProvider.addItem(item1);
        stubbedOfflineContentProvider.addItem(item2);
        stubbedOfflineContentProvider.addItem(item3);

        TrackerFactory.setTrackerForTests(mTracker);
    }

    private void setUpUi() {
        FilterCoordinator.setPrefetchUserSettingValueForTesting(true);
        DownloadManagerUiConfig config = new DownloadManagerUiConfig.Builder()
                                                 .setIsOffTheRecord(false)
                                                 .setIsSeparateActivity(true)
                                                 .setUseNewDownloadPath(true)
                                                 .setUseNewDownloadPathThumbnails(true)
                                                 .build();

        mModalDialogManager =
                new ModalDialogManager(mAppModalPresenter, ModalDialogManager.ModalDialogType.APP);

        mDownloadCoordinator = new DownloadManagerCoordinatorImpl(
                mProfile, getActivity(), config, mSnackbarManager, mModalDialogManager);
        getActivity().setContentView(mDownloadCoordinator.getView());

        mDownloadCoordinator.updateForUrl(UrlConstants.DOWNLOADS_URL);
    }

    @Test
    @MediumTest
    public void testLaunchingActivity() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setUpUi(); });

        onView(withText("page 1")).check(matches(isDisplayed()));
        onView(withText("page 2")).check(matches(isDisplayed()));
        onView(withText("page 3")).check(matches(isDisplayed()));
        onView(withText("page 4")).check(matches(isDisplayed()));
    }
}
