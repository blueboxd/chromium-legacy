// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.longClick;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import android.support.test.filters.SmallTest;
import android.view.ContextMenu;
import android.view.MenuItem;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.homepage.HomepageSettings;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 * Test related to {@link HomeButton}.
 *
 * Currently the change only affects {@link
 * FeatureUtilities#isHomepageSettingsUIConversionEnabled()}.
 * TODO: Add more test when features related has update.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class HomeButtonTest extends DummyUiActivityTestCase {
    private static final String ASSERT_MSG_MENU_IS_CREATED =
            "ContextMenu is not created after long press.";
    private static final String ASSERT_MSG_MENU_SIZE =
            "ContextMenu has a different size than test setting.";
    private static final String ASSERT_MSG_MENU_ITEM_TEXT =
            "MenuItem does shows different text than test setting.";

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private SettingsLauncher mSettingsLauncher;

    private HomeButton mHomeButton;
    private int mIdHomeButton;

    private boolean mIsHomepageEnabled;
    private boolean mIsUsingDefaultHomepage;
    private String mCustomizedHomepage;

    public HomeButtonTest() {
        super();

        SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        mIsHomepageEnabled = manager.readBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        mIsUsingDefaultHomepage =
                manager.readBoolean(ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, true);
        mCustomizedHomepage = manager.readString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_URI, "");
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        MockitoAnnotations.initMocks(this);
        SettingsLauncher.getInstance().setInstanceForTests(mSettingsLauncher);

        setupDefaultHomepagePreferences();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FrameLayout content = new FrameLayout(getActivity());
            getActivity().setContentView(content);

            mIdHomeButton = View.generateViewId();
            mHomeButton = new HomeButton(getActivity(), null);
            mHomeButton.setId(mIdHomeButton);
            HomeButton.setSaveContextMenuForTests(true);

            content.addView(mHomeButton);
        });
    }

    @Override
    public void tearDownTest() throws Exception {
        super.tearDownTest();
        resetHomepageRelatedPreferenceAfterTest();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.HOMEPAGE_SETTINGS_UI_CONVERSION)
    public void testContextMenu_BeforeConversion() {
        onView(withId(mIdHomeButton)).perform(longClick());

        ContextMenu menu = mHomeButton.getMenuForTests();
        Assert.assertNotNull(ASSERT_MSG_MENU_IS_CREATED, menu);
        Assert.assertEquals(ASSERT_MSG_MENU_SIZE, 1, menu.size());

        MenuItem item_remove = menu.findItem(HomeButton.ID_REMOVE);
        Assert.assertNotNull("MenuItem 'Remove' is not added to menu", item_remove);
        Assert.assertEquals(ASSERT_MSG_MENU_ITEM_TEXT, item_remove.getTitle().toString(),
                getActivity().getResources().getString(R.string.remove));

        // Test click on context menu item
        Assert.assertTrue("Homepage should be enabled before clicking the menu item.",
                HomepageManager.isHomepageEnabled());
        onView(withText(R.string.remove)).perform(click());
        Assert.assertFalse("Homepage should be disabled after clicking the menu item.",
                HomepageManager.isHomepageEnabled());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.HOMEPAGE_SETTINGS_UI_CONVERSION)
    public void testContextMenu_AfterConversion() {
        onView(withId(mIdHomeButton)).perform(longClick());

        ContextMenu menu = mHomeButton.getMenuForTests();
        Assert.assertNotNull(ASSERT_MSG_MENU_IS_CREATED, menu);
        Assert.assertEquals(ASSERT_MSG_MENU_SIZE, 1, menu.size());

        MenuItem item_settings = menu.findItem(HomeButton.ID_SETTINGS);
        Assert.assertNotNull("MenuItem 'Edit Homepage' is not added to menu", item_settings);
        Assert.assertEquals(ASSERT_MSG_MENU_ITEM_TEXT, item_settings.getTitle().toString(),
                getActivity().getResources().getString(R.string.homebutton_context_menu_settings));

        // Test click on context menu item
        onView(withText(R.string.homebutton_context_menu_settings)).perform(click());
        Mockito.verify(mSettingsLauncher)
                .launchSettingsPage(mHomeButton.getContext(), HomepageSettings.class);
    }

    private void resetHomepageRelatedPreferenceAfterTest() {
        SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        manager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, mIsHomepageEnabled);
        manager.writeBoolean(
                ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, mIsUsingDefaultHomepage);
        manager.writeString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_URI, mCustomizedHomepage);
    }

    /**
     * Set the default test status for homepage button tests.
     * By default, the homepage is <b>enabled</b> and with customized url
     * <b>https://www.chromium.org/</b>.
     */
    private void setupDefaultHomepagePreferences() {
        SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        manager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        manager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, false);
        manager.writeString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_URI, "https://www.chromium.org/");
    }
}
