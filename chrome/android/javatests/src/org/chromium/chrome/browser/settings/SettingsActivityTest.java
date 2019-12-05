// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.settings.website.ContentSettingValues;
import org.chromium.chrome.browser.settings.website.PermissionInfo;
import org.chromium.chrome.browser.settings.website.WebsitePreferenceBridgeJni;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.LoadListener;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.policy.test.annotations.Policies;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for the Settings menu.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures(ChromeFeatureList.CAPTION_SETTINGS)
public class SettingsActivityTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    /**
     * Launches the settings activity with the specified fragment.
     * Returns the activity that was started.
     */
    public static SettingsActivity startSettingsActivity(
            Instrumentation instrumentation, String fragmentName) {
        Context context = instrumentation.getTargetContext();
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(context, fragmentName);
        Activity activity = instrumentation.startActivitySync(intent);
        Assert.assertTrue(activity instanceof SettingsActivity);
        return (SettingsActivity) activity;
    }

    /**
     * Change search engine and make sure it works correctly.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableIf.Build(hardware_is = "sprout", message = "crashes on android-one: crbug.com/540720")
    @RetryOnFailure
    public void testSearchEnginePreference() throws Exception {
        ensureTemplateUrlServiceLoaded();

        final SettingsActivity settingsActivity =
                startSettingsActivity(InstrumentationRegistry.getInstrumentation(),
                        SearchEnginePreference.class.getName());

        // Set the second search engine as the default using TemplateUrlService.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SearchEnginePreference pref =
                    (SearchEnginePreference) settingsActivity.getMainFragment();
            pref.setValueForTesting("1");

            // Ensure that the second search engine in the list is selected.
            Assert.assertNotNull(pref);
            Assert.assertEquals("1", pref.getValueForTesting());

            // Simulate selecting the third search engine, ensure that TemplateUrlService is
            // updated, and location permission granted by default for the new engine.
            String keyword2 = pref.setValueForTesting("2");
            TemplateUrlService templateUrlService = TemplateUrlServiceFactory.get();
            Assert.assertEquals(
                    keyword2, templateUrlService.getDefaultSearchEngineTemplateUrl().getKeyword());
            Assert.assertEquals(
                    ContentSettingValues.ALLOW, locationPermissionForSearchEngine(keyword2));

            // Simulate selecting the fourth search engine and but set a blocked permission
            // first and ensure that location permission is NOT granted.
            String keyword3 = pref.getKeywordFromIndexForTesting(3);
            String url = templateUrlService.getSearchEngineUrlFromTemplateUrl(keyword3);
            WebsitePreferenceBridgeJni.get().setGeolocationSettingForOrigin(
                    url, url, ContentSettingValues.BLOCK, false);
            keyword3 = pref.setValueForTesting("3");
            Assert.assertEquals(keyword3,
                    TemplateUrlServiceFactory.get()
                            .getDefaultSearchEngineTemplateUrl()
                            .getKeyword());
            Assert.assertEquals(
                    ContentSettingValues.BLOCK, locationPermissionForSearchEngine(keyword3));
            Assert.assertEquals(
                    ContentSettingValues.ASK, locationPermissionForSearchEngine(keyword2));

            // Make sure a pre-existing ALLOW value does not get deleted when switching away
            // from a search engine. For this to work we need to change the DSE's content
            // setting to allow for search engine 3 before changing to search engine 2.
            // Otherwise the block setting will cause the content setting for search engine 2
            // to be reset when we switch to it.
            WebsitePreferenceBridgeJni.get().setGeolocationSettingForOrigin(
                    url, url, ContentSettingValues.ALLOW, false);
            keyword2 = pref.getKeywordFromIndexForTesting(2);
            url = templateUrlService.getSearchEngineUrlFromTemplateUrl(keyword2);
            WebsitePreferenceBridgeJni.get().setGeolocationSettingForOrigin(
                    url, url, ContentSettingValues.ALLOW, false);
            keyword2 = pref.setValueForTesting("2");
            Assert.assertEquals(keyword2,
                    TemplateUrlServiceFactory.get()
                            .getDefaultSearchEngineTemplateUrl()
                            .getKeyword());

            Assert.assertEquals(
                    ContentSettingValues.ALLOW, locationPermissionForSearchEngine(keyword2));
            pref.setValueForTesting("3");
            Assert.assertEquals(
                    ContentSettingValues.ALLOW, locationPermissionForSearchEngine(keyword2));
        });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "DefaultSearchProviderEnabled", string = "false") })
    public void testSearchEnginePreference_DisabledIfNoDefaultSearchEngine() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeBrowserInitializer.getInstance(InstrumentationRegistry.getTargetContext())
                    .handleSynchronousStartup();
        });

        ensureTemplateUrlServiceLoaded();
        CriteriaHelper.pollUiThread(Criteria.equals(true, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return TemplateUrlServiceFactory.get().isDefaultSearchManaged();
            }
        }));

        SettingsActivity settingsActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SettingsActivity.class);

        final MainPreferences mainPreferences =
                ActivityUtils.waitForFragmentToAttach(settingsActivity, MainPreferences.class);

        final Preference searchEnginePref =
                waitForPreference(mainPreferences, MainPreferences.PREF_SEARCH_ENGINE);

        CriteriaHelper.pollUiThread(Criteria.equals(null, new Callable<Object>() {
            @Override
            public Object call() {
                return searchEnginePref.getFragment();
            }
        }));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ManagedPreferenceDelegate managedPrefDelegate =
                    mainPreferences.getManagedPreferenceDelegateForTest();
            Assert.assertTrue(managedPrefDelegate.isPreferenceControlledByPolicy(searchEnginePref));
        });
    }

    /**
     * Make sure that when a user switches to a search engine that uses HTTP, the location
     * permission is not added.
     */
    /*
     * @SmallTest
     * @Feature({"Preferences"})
     * BUG=crbug.com/540706
     */
    @Test
    @FlakyTest
    @DisableIf.Build(hardware_is = "sprout", message = "fails on android-one: crbug.com/540706")
    public void testSearchEnginePreferenceHttp() throws Exception {
        ensureTemplateUrlServiceLoaded();

        final SettingsActivity settingsActivity =
                startSettingsActivity(InstrumentationRegistry.getInstrumentation(),
                        SearchEnginePreference.class.getName());

        // Set the first search engine as the default using TemplateUrlService.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SearchEnginePreference pref =
                    (SearchEnginePreference) settingsActivity.getMainFragment();
            pref.setValueForTesting("0");
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Ensure that the first search engine in the list is selected.
            SearchEnginePreference pref =
                    (SearchEnginePreference) settingsActivity.getMainFragment();
            Assert.assertNotNull(pref);
            Assert.assertEquals("0", pref.getValueForTesting());

            // Simulate selecting a search engine that uses HTTP.
            int index = indexOfFirstHttpSearchEngine(pref);
            String keyword = pref.setValueForTesting(Integer.toString(index));

            TemplateUrlService templateUrlService = TemplateUrlServiceFactory.get();
            Assert.assertEquals(
                    keyword, templateUrlService.getDefaultSearchEngineTemplateUrl().getKeyword());
            Assert.assertEquals(
                    ContentSettingValues.ASK, locationPermissionForSearchEngine(keyword));
        });
    }

    private int indexOfFirstHttpSearchEngine(SearchEnginePreference pref) {
        TemplateUrlService templateUrlService = TemplateUrlServiceFactory.get();
        List<TemplateUrl> urls = templateUrlService.getTemplateUrls();
        int index;
        for (index = 0; index < urls.size(); ++index) {
            String keyword = pref.getKeywordFromIndexForTesting(index);
            String url = templateUrlService.getSearchEngineUrlFromTemplateUrl(keyword);
            if (url.startsWith("http:")) {
                return index;
            }
        }
        Assert.fail();
        return index;
    }

    private void ensureTemplateUrlServiceLoaded() throws Exception {
        // Make sure the template_url_service is loaded.
        final CallbackHelper onTemplateUrlServiceLoadedHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            if (TemplateUrlServiceFactory.get().isLoaded()) {
                onTemplateUrlServiceLoadedHelper.notifyCalled();
            } else {
                TemplateUrlServiceFactory.get().registerLoadListener(new LoadListener() {
                    @Override
                    public void onTemplateUrlServiceLoaded() {
                        onTemplateUrlServiceLoadedHelper.notifyCalled();
                    }
                });
                TemplateUrlServiceFactory.get().load();
            }
        });
        onTemplateUrlServiceLoadedHelper.waitForCallback(0);
    }

    private @ContentSettingValues int locationPermissionForSearchEngine(String keyword) {
        String url = TemplateUrlServiceFactory.get().getSearchEngineUrlFromTemplateUrl(keyword);
        PermissionInfo locationSettings =
                new PermissionInfo(PermissionInfo.Type.GEOLOCATION, url, null, false);
        @ContentSettingValues
        int locationPermission = locationSettings.getContentSetting();
        return locationPermission;
    }

    @Test
    @SmallTest
    @Policies.Add({ @Policies.Item(key = "PasswordManagerEnabled", string = "false") })
    public void testSavePasswordsPreferences_ManagedAndDisabled() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeBrowserInitializer.getInstance().handleSynchronousStartup(); });

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return PrefServiceBridge.getInstance().isManagedPreference(
                        Pref.REMEMBER_PASSWORDS_ENABLED);
            }
        });

        SettingsActivityTest.startSettingsActivity(
                InstrumentationRegistry.getInstrumentation(), MainPreferences.class.getName());

        onView(withText(R.string.prefs_saved_passwords_title)).perform(click());
        onView(withText(R.string.prefs_saved_passwords)).check(matches(isDisplayed()));
    }

    private static Preference waitForPreference(final PreferenceFragmentCompat prefFragment,
            final String preferenceKey) throws ExecutionException {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return prefFragment.findPreference(preferenceKey) != null;
            }
        });

        return TestThreadUtils.runOnUiThreadBlocking(
                () -> prefFragment.findPreference(preferenceKey));
    }
}
