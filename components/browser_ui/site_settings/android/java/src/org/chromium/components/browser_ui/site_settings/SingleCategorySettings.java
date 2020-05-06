// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.browser_ui.settings.SearchUtils.handleSearchNavigation;
import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import android.content.Context;
import android.content.DialogInterface;
import android.os.Build;
import android.os.Bundle;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.format.Formatter;
import android.text.style.ForegroundColorSpan;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.preference.Preference;
import androidx.preference.PreferenceGroup;
import androidx.preference.PreferenceManager;
import androidx.preference.PreferenceScreen;
import androidx.recyclerview.widget.RecyclerView;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.components.browser_ui.settings.ChromeBaseCheckBoxPreference;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.settings.SearchUtils;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.FourStateCookieSettingsPreference.CookieSettingsState;
import org.chromium.components.browser_ui.site_settings.Website.StoredDataClearedCallback;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsFeatureList;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Shows a list of sites in a particular Site Settings category. For example, this could show all
 * the websites with microphone permissions. When the user selects a site, SingleWebsiteSettings
 * is launched to allow the user to see or modify the settings for that particular website.
 */
public class SingleCategorySettings extends SiteSettingsPreferenceFragment
        implements Preference.OnPreferenceChangeListener, Preference.OnPreferenceClickListener,
                   AddExceptionPreference.SiteAddedCallback, View.OnClickListener,
                   PreferenceManager.OnPreferenceTreeClickListener {
    // The key to use to pass which category this preference should display,
    // e.g. Location/Popups/All sites (if blank).
    public static final String EXTRA_CATEGORY = "category";
    public static final String EXTRA_TITLE = "title";

    /**
     * If present, the list of websites will be filtered by domain using
     * {@link UrlUtilities#getDomainAndRegistry}.
     */
    public static final String EXTRA_SELECTED_DOMAINS = "selected_domains";

    // The list that contains preferences.
    private RecyclerView mListView;
    // The view to show when the list is empty.
    private TextView mEmptyView;
    // The item for searching the list of items.
    private MenuItem mSearchItem;
    // The clear button displayed in the Storage view.
    private Button mClearButton;
    // The Site Settings Category we are showing.
    private SiteSettingsCategory mCategory;
    // If not blank, represents a substring to use to search for site names.
    private String mSearch;
    // Whether to group by allowed/blocked list.
    private boolean mGroupByAllowBlock;
    // Whether the Blocked list should be shown expanded.
    private boolean mBlockListExpanded;
    // Whether the Allowed list should be shown expanded.
    private boolean mAllowListExpanded = true;
    // Whether the Managed list should be shown expanded.
    private boolean mManagedListExpanded;
    // Whether this is the first time this screen is shown.
    private boolean mIsInitialRun = true;
    // The number of sites that are on the Allowed list.
    private int mAllowedSiteCount;
    // The websites that are currently displayed to the user.
    private List<WebsitePreference> mWebsites;
    // Whether tri-state ContentSetting is required.
    private boolean mRequiresTriStateSetting;
    // Whether four-state ContentSetting is required.
    private boolean mRequiresFourStateSetting;
    // Locally-saved reference to the "notifications_quiet_ui" preference to allow hiding/showing
    // it.
    private ChromeBaseCheckBoxPreference mNotificationsQuietUiPref;

    @Nullable
    private Set<String> mSelectedDomains;

    // Keys for common ContentSetting toggle for categories. These three toggles are mutually
    // exclusive: a category should only show one of them, at most.
    public static final String BINARY_TOGGLE_KEY = "binary_toggle";
    public static final String TRI_STATE_TOGGLE_KEY = "tri_state_toggle";
    public static final String FOUR_STATE_COOKIE_TOGGLE_KEY = "four_state_cookie_toggle";

    // Keys for category-specific preferences (toggle, link, button etc.), dynamically shown.
    public static final String THIRD_PARTY_COOKIES_TOGGLE_KEY = "third_party_cookies";
    public static final String NOTIFICATIONS_VIBRATE_TOGGLE_KEY = "notifications_vibrate";
    public static final String NOTIFICATIONS_QUIET_UI_TOGGLE_KEY = "notifications_quiet_ui";
    public static final String EXPLAIN_PROTECTED_MEDIA_KEY = "protected_content_learn_more";
    private static final String ADD_EXCEPTION_KEY = "add_exception";
    public static final String COOKIE_INFO_TEXT_KEY = "cookie_info_text";

    // Keys for Allowed/Blocked preference groups/headers.
    private static final String ALLOWED_GROUP = "allowed_group";
    private static final String BLOCKED_GROUP = "blocked_group";
    private static final String MANAGED_GROUP = "managed_group";

    private class ResultsPopulator implements WebsitePermissionsFetcher.WebsitePermissionsCallback {
        @Override
        public void onWebsitePermissionsAvailable(Collection<Website> sites) {
            // This method may be called after the activity has been destroyed.
            // In that case, bail out.
            if (getActivity() == null) return;
            mWebsites = null;

            resetList();

            int chooserDataType = mCategory.getObjectChooserDataType();
            boolean hasEntries =
                    chooserDataType == -1 ? addWebsites(sites) : addChosenObjects(sites);

            if (mEmptyView == null) return;

            mEmptyView.setVisibility(hasEntries ? View.GONE : View.VISIBLE);
        }
    }

    /** Called by common settings code to determine if a Preference is managed. */
    private class SingleCategoryManagedPreferenceDelegate
            extends ForwardingManagedPreferenceDelegate {
        SingleCategoryManagedPreferenceDelegate(ManagedPreferenceDelegate base) {
            super(base);
        }

        @Override
        public boolean isPreferenceControlledByPolicy(Preference preference) {
            // TODO(bauerb): Align the ManagedPreferenceDelegate and
            // SiteSettingsCategory interfaces better to avoid this indirection.
            return mCategory.isManaged() && !mCategory.isManagedByCustodian();
        }

        @Override
        public boolean isPreferenceControlledByCustodian(Preference preference) {
            return mCategory.isManagedByCustodian();
        }
    }

    private void getInfoForOrigins() {
        if (!mCategory.enabledInAndroid(getActivity())) {
            // No need to fetch any data if we're not going to show it, but we do need to update
            // the global toggle to reflect updates in Android settings (e.g. Location).
            resetList();
            return;
        }

        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(
                getSiteSettingsClient().getBrowserContextHandle(), false);
        fetcher.fetchPreferencesForCategory(mCategory, new ResultsPopulator());
    }

    /**
     * Returns whether a website is on the Blocked list for the category currently showing.
     * @param website The website to check.
     */
    private boolean isOnBlockList(WebsitePreference website) {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsClient().getBrowserContextHandle();
        for (@SiteSettingsCategory.Type int i = 0; i < SiteSettingsCategory.Type.NUM_ENTRIES; i++) {
            if (!mCategory.showSites(i)) continue;
            for (@ContentSettingException.Type int j = 0;
                    j < ContentSettingException.Type.NUM_ENTRIES; j++) {
                if (ContentSettingException.getContentSettingsType(j)
                        == SiteSettingsCategory.contentSettingsType(i)) {
                    return ContentSettingValues.BLOCK
                            == website.site().getContentSettingPermission(j);
                }
            }
            for (@PermissionInfo.Type int j = 0; j < PermissionInfo.Type.NUM_ENTRIES; j++) {
                if (PermissionInfo.getContentSettingsType(j)
                        == SiteSettingsCategory.contentSettingsType(i)) {
                    return j != PermissionInfo.Type.MIDI
                            && ContentSettingValues.BLOCK
                            == website.site().getPermission(browserContextHandle, j);
                }
            }
        }
        return false;
    }

    /**
     * Update the Category Header for the Allowed list.
     * @param numAllowed The number of sites that are on the Allowed list
     * @param toggleValue The value the global toggle will have once precessing ends.
     */
    private void updateAllowedHeader(int numAllowed, boolean toggleValue) {
        ExpandablePreferenceGroup allowedGroup =
                (ExpandablePreferenceGroup) getPreferenceScreen().findPreference(ALLOWED_GROUP);
        if (allowedGroup == null) return;

        if (numAllowed == 0) {
            if (allowedGroup != null) getPreferenceScreen().removePreference(allowedGroup);
            return;
        }
        if (!mGroupByAllowBlock) return;

        // When the toggle is set to Blocked, the Allowed list header should read 'Exceptions', not
        // 'Allowed' (because it shows exceptions from the rule).
        int resourceId = toggleValue ? R.string.website_settings_allowed_group_heading
                                     : R.string.website_settings_exceptions_group_heading;
        allowedGroup.setTitle(getHeaderTitle(resourceId, numAllowed));
        allowedGroup.setExpanded(mAllowListExpanded);
    }

    private void updateBlockedHeader(int numBlocked) {
        ExpandablePreferenceGroup blockedGroup =
                (ExpandablePreferenceGroup) getPreferenceScreen().findPreference(BLOCKED_GROUP);
        if (numBlocked == 0) {
            if (blockedGroup != null) getPreferenceScreen().removePreference(blockedGroup);
            return;
        }
        if (!mGroupByAllowBlock) return;

        // Set the title and arrow icons for the header.
        int resourceId = mCategory.showSites(SiteSettingsCategory.Type.SOUND)
                ? R.string.website_settings_blocked_group_heading_sound
                : R.string.website_settings_blocked_group_heading;
        blockedGroup.setTitle(getHeaderTitle(resourceId, numBlocked));
        blockedGroup.setExpanded(mBlockListExpanded);
    }

    private void updateManagedHeader(int numManaged) {
        ExpandablePreferenceGroup managedGroup =
                (ExpandablePreferenceGroup) getPreferenceScreen().findPreference(MANAGED_GROUP);
        if (numManaged == 0) {
            if (managedGroup != null) getPreferenceScreen().removePreference(managedGroup);
            return;
        }
        if (!mGroupByAllowBlock) return;

        // Set the title and arrow icons for the header.
        int resourceId = R.string.website_settings_managed_group_heading;
        managedGroup.setTitle(getHeaderTitle(resourceId, numManaged));
        managedGroup.setExpanded(mManagedListExpanded);
    }

    private CharSequence getHeaderTitle(int resourceId, int count) {
        SpannableStringBuilder spannable = new SpannableStringBuilder(getString(resourceId));
        String prefCount = String.format(Locale.getDefault(), " - %d", count);
        spannable.append(prefCount);

        // Color the first part of the title blue.
        ForegroundColorSpan blueSpan = new ForegroundColorSpan(
                ApiCompatibilityUtils.getColor(getResources(), R.color.default_text_color_link));
        spannable.setSpan(blueSpan, 0, spannable.length() - prefCount.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

        // Gray out the total count of items.
        int gray = ApiCompatibilityUtils.getColor(
                getResources(), R.color.default_text_color_secondary);
        spannable.setSpan(new ForegroundColorSpan(gray), spannable.length() - prefCount.length(),
                spannable.length(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        return spannable;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // Read which category we should be showing.
        BrowserContextHandle browserContextHandle =
                getSiteSettingsClient().getBrowserContextHandle();
        if (getArguments() != null) {
            mCategory = SiteSettingsCategory.createFromPreferenceKey(
                    browserContextHandle, getArguments().getString(EXTRA_CATEGORY, ""));
        }
        if (mCategory == null) {
            mCategory = SiteSettingsCategory.createFromType(
                    browserContextHandle, SiteSettingsCategory.Type.ALL_SITES);
        }

        int contentType = mCategory.getContentSettingsType();
        mRequiresTriStateSetting =
                WebsitePreferenceBridge.requiresTriStateContentSetting(contentType);
        mRequiresFourStateSetting =
                WebsitePreferenceBridge.requiresFourStateContentSetting(contentType);

        ViewGroup view = (ViewGroup) super.onCreateView(inflater, container, savedInstanceState);

        // Add custom views for Storage Preferences to bottom of the fragment.
        if (mCategory.showSites(SiteSettingsCategory.Type.USE_STORAGE)) {
            inflater.inflate(R.layout.storage_preferences_view, view, true);
            mEmptyView = view.findViewById(R.id.empty_storage);
            mClearButton = view.findViewById(R.id.clear_button);
            mClearButton.setOnClickListener(this);
        }

        mListView = getListView();

        // Disable animations of preference changes.
        mListView.setItemAnimator(null);

        // Remove dividers between preferences.
        setDivider(null);

        return view;
    }

    /**
     * Returns the category being displayed. For testing.
     */
    public SiteSettingsCategory getCategoryForTest() {
        return mCategory;
    }

    /**
     * This clears all the storage for websites that are displayed to the user. This happens
     * asynchronously, and then we call {@link #getInfoForOrigins()} when we're done.
     */
    public void clearStorage() {
        if (mWebsites == null) return;
        RecordUserAction.record("MobileSettingsStorageClearAll");

        // The goal is to refresh the info for origins again after we've cleared all of them, so we
        // wait until the last website is cleared to refresh the origin list.
        final int[] numLeft = new int[1];
        numLeft[0] = mWebsites.size();
        for (int i = 0; i < mWebsites.size(); i++) {
            WebsitePreference preference = mWebsites.get(i);
            preference.site().clearAllStoredData(getSiteSettingsClient().getBrowserContextHandle(),
                    new StoredDataClearedCallback() {
                        @Override
                        public void onStoredDataCleared() {
                            if (--numLeft[0] <= 0) getInfoForOrigins();
                        }
                    });
        }
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        // Handled in onActivityCreated. Moving the addPreferencesFromResource call up to here
        // causes animation jank (crbug.com/985734).
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.website_preferences);

        String title = getArguments().getString(EXTRA_TITLE);
        if (title != null) getActivity().setTitle(title);

        mSelectedDomains = getArguments().containsKey(EXTRA_SELECTED_DOMAINS)
                ? new HashSet<>(getArguments().getStringArrayList(EXTRA_SELECTED_DOMAINS))
                : null;

        configureGlobalToggles();

        setHasOptionsMenu(true);

        super.onActivityCreated(savedInstanceState);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        inflater.inflate(R.menu.website_preferences_menu, menu);

        mSearchItem = menu.findItem(R.id.search);
        SearchUtils.initializeSearchView(mSearchItem, mSearch, getActivity(), (query) -> {
            boolean queryHasChanged =
                    mSearch == null ? query != null && !query.isEmpty() : !mSearch.equals(query);
            mSearch = query;
            if (queryHasChanged) getInfoForOrigins();
        });

        if (getSiteSettingsClient().getSiteSettingsHelpClient().isHelpAndFeedbackEnabled()) {
            MenuItem help = menu.add(
                    Menu.NONE, R.id.menu_id_site_settings_help, Menu.NONE, R.string.menu_help);
            help.setIcon(VectorDrawableCompat.create(
                    getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_site_settings_help) {
            if (mCategory.showSites(SiteSettingsCategory.Type.PROTECTED_MEDIA)) {
                getSiteSettingsClient()
                        .getSiteSettingsHelpClient()
                        .launchProtectedContentHelpAndFeedbackActivity(getActivity());
            } else {
                getSiteSettingsClient()
                        .getSiteSettingsHelpClient()
                        .launchSettingsHelpAndFeedbackActivity(getActivity());
            }
            return true;
        }
        if (handleSearchNavigation(item, mSearchItem, mSearch, getActivity())) {
            boolean queryHasChanged = mSearch != null && !mSearch.isEmpty();
            mSearch = null;
            if (queryHasChanged) getInfoForOrigins();
            return true;
        }
        return false;
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        // Do not show the toast if the System Location setting is disabled.
        if (getPreferenceScreen().findPreference(BINARY_TOGGLE_KEY) != null
                && mCategory.isManaged()) {
            showManagedToast();
            return false;
        }

        if (preference instanceof WebsitePreference) {
            WebsitePreference website = (WebsitePreference) preference;
            website.setFragment(SingleWebsiteSettings.class.getName());
            // EXTRA_SITE re-uses already-fetched permissions, which we can only use if the Website
            // was populated with data for all permission types.
            if (mCategory.showSites(SiteSettingsCategory.Type.ALL_SITES)) {
                website.putSiteIntoExtras(SingleWebsiteSettings.EXTRA_SITE);
            } else {
                website.putSiteAddressIntoExtras(SingleWebsiteSettings.EXTRA_SITE_ADDRESS);
            }
            int navigationSource = getArguments().getInt(
                    SettingsNavigationSource.EXTRA_KEY, SettingsNavigationSource.OTHER);
            website.getExtras().putInt(SettingsNavigationSource.EXTRA_KEY, navigationSource);
        }

        return super.onPreferenceTreeClick(preference);
    }

    /** OnClickListener for the clear button. We show an alert dialog to confirm the action */
    @Override
    public void onClick(View v) {
        if (getActivity() == null || v != mClearButton) return;

        long totalUsage = 0;
        boolean includesApps = false;
        Set<String> originsWithInstalledApp =
                getSiteSettingsClient().getWebappSettingsClient().getOriginsWithInstalledApp();
        if (mWebsites != null) {
            for (WebsitePreference preference : mWebsites) {
                totalUsage += preference.site().getTotalUsage();
                if (!includesApps) {
                    includesApps = originsWithInstalledApp.contains(
                            preference.site().getAddress().getOrigin());
                }
            }
        }

        AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
        View dialogView =
                getActivity().getLayoutInflater().inflate(R.layout.clear_data_dialog, null);
        TextView message = dialogView.findViewById(android.R.id.message);
        TextView signedOutText = dialogView.findViewById(R.id.signed_out_text);
        TextView offlineText = dialogView.findViewById(R.id.offline_text);
        signedOutText.setText(R.string.webstorage_clear_data_dialog_sign_out_all_message);
        offlineText.setText(R.string.webstorage_clear_data_dialog_offline_message);
        String dialogFormattedText =
                getString(includesApps ? R.string.webstorage_clear_data_dialog_message_with_app
                                       : R.string.webstorage_clear_data_dialog_message,
                        Formatter.formatShortFileSize(getActivity(), totalUsage));
        message.setText(dialogFormattedText);
        builder.setView(dialogView);
        builder.setPositiveButton(R.string.storage_clear_dialog_clear_storage_option,
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        clearStorage();
                    }
                });
        builder.setNegativeButton(R.string.cancel, null);
        builder.setTitle(R.string.storage_clear_site_storage_title);
        builder.create().show();
    }

    // OnPreferenceChangeListener:
    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsClient().getBrowserContextHandle();
        if (BINARY_TOGGLE_KEY.equals(preference.getKey())) {
            assert !mCategory.isManaged();

            for (@SiteSettingsCategory.Type int type = 0;
                    type < SiteSettingsCategory.Type.NUM_ENTRIES; type++) {
                if (type == SiteSettingsCategory.Type.ALL_SITES
                        || type == SiteSettingsCategory.Type.USE_STORAGE
                        || !mCategory.showSites(type)) {
                    continue;
                }

                WebsitePreferenceBridge.setCategoryEnabled(browserContextHandle,
                        SiteSettingsCategory.contentSettingsType(type), (boolean) newValue);

                if (type == SiteSettingsCategory.Type.COOKIES) {
                    updateThirdPartyCookiesCheckBox();
                } else if (type == SiteSettingsCategory.Type.NOTIFICATIONS) {
                    updateNotificationsSecondaryControls();
                }
                break;
            }

            getInfoForOrigins();
        } else if (TRI_STATE_TOGGLE_KEY.equals(preference.getKey())) {
            @ContentSettingValues
            int setting = (int) newValue;
            WebsitePreferenceBridge.setContentSetting(
                    browserContextHandle, mCategory.getContentSettingsType(), setting);
            getInfoForOrigins();
        } else if (FOUR_STATE_COOKIE_TOGGLE_KEY.equals(preference.getKey())) {
            setCookieSettingsPreference((CookieSettingsState) newValue);
            getInfoForOrigins();
        } else if (THIRD_PARTY_COOKIES_TOGGLE_KEY.equals(preference.getKey())) {
            getPrefs().setBlockThirdPartyCookies((boolean) newValue);
        } else if (NOTIFICATIONS_VIBRATE_TOGGLE_KEY.equals(preference.getKey())) {
            getPrefs().setNotificationsVibrateEnabled((boolean) newValue);
        } else if (NOTIFICATIONS_QUIET_UI_TOGGLE_KEY.equals(preference.getKey())) {
            boolean boolValue = (boolean) newValue;
            if (boolValue) {
                getPrefs().setEnableQuietNotificationPermissionUi(true);
            } else {
                // Clear the pref so if the default changes later the user will get the new default.
                getPrefs().clearEnableNotificationPermissionUi();
            }
        }
        return true;
    }

    private void setCookieSettingsPreference(CookieSettingsState state) {
        boolean allowCookies;
        @CookieControlsMode
        int mode;

        switch (state) {
            case ALLOW:
                allowCookies = true;
                mode = CookieControlsMode.OFF;
                break;
            case BLOCK_THIRD_PARTY_INCOGNITO:
                allowCookies = true;
                mode = CookieControlsMode.INCOGNITO_ONLY;
                break;
            case BLOCK_THIRD_PARTY:
                allowCookies = true;
                mode = CookieControlsMode.ON;
                break;
            case BLOCK:
                allowCookies = false;
                mode = CookieControlsMode.ON;
                break;
            default:
                return;
        }

        WebsitePreferenceBridge.setCategoryEnabled(
                getSiteSettingsClient().getBrowserContextHandle(), ContentSettingsType.COOKIES,
                allowCookies);
        getPrefs().setCookieControlsMode(mode);
        getPrefs().setBlockThirdPartyCookies(mode == CookieControlsMode.ON);
    }

    private boolean cookieSettingsExceptionShouldBlock() {
        FourStateCookieSettingsPreference fourStateCookieToggle =
                (FourStateCookieSettingsPreference) getPreferenceScreen().findPreference(
                        FOUR_STATE_COOKIE_TOGGLE_KEY);
        return fourStateCookieToggle.getState() == CookieSettingsState.ALLOW;
    }

    private String getAddExceptionDialogMessage() {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsClient().getBrowserContextHandle();
        int resource = 0;
        if (mCategory.showSites(SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS)) {
            resource = R.string.website_settings_add_site_description_automatic_downloads;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.BACKGROUND_SYNC)) {
            resource = R.string.website_settings_add_site_description_background_sync;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.JAVASCRIPT)) {
            resource = WebsitePreferenceBridge.isCategoryEnabled(
                               browserContextHandle, ContentSettingsType.JAVASCRIPT)
                    ? R.string.website_settings_add_site_description_javascript_block
                    : R.string.website_settings_add_site_description_javascript_allow;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.SOUND)) {
            resource = WebsitePreferenceBridge.isCategoryEnabled(
                               browserContextHandle, ContentSettingsType.SOUND)
                    ? R.string.website_settings_add_site_description_sound_block
                    : R.string.website_settings_add_site_description_sound_allow;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.COOKIES)) {
            if (mRequiresFourStateSetting) {
                resource = cookieSettingsExceptionShouldBlock()
                        ? R.string.website_settings_add_site_description_cookies_block
                        : R.string.website_settings_add_site_description_cookies_allow;
            } else {
                resource = WebsitePreferenceBridge.isCategoryEnabled(
                                   browserContextHandle, ContentSettingsType.COOKIES)
                        ? R.string.website_settings_add_site_description_cookies_block
                        : R.string.website_settings_add_site_description_cookies_allow;
            }
        }
        assert resource > 0;
        return getString(resource);
    }

    // OnPreferenceClickListener:
    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (ALLOWED_GROUP.equals(preference.getKey())) {
            mAllowListExpanded = !mAllowListExpanded;
        } else if (BLOCKED_GROUP.equals(preference.getKey())) {
            mBlockListExpanded = !mBlockListExpanded;
        } else {
            mManagedListExpanded = !mManagedListExpanded;
        }
        getInfoForOrigins();
        return true;
    }

    @Override
    public void onResume() {
        super.onResume();

        if (mSearch == null && mSearchItem != null) {
            SearchUtils.clearSearch(mSearchItem, getActivity());
            mSearch = null;
        }

        getInfoForOrigins();
    }

    // AddExceptionPreference.SiteAddedCallback:
    @Override
    public void onAddSite(String primaryPattern, String secondaryPattern) {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsClient().getBrowserContextHandle();
        int setting;
        if (mCategory.showSites(SiteSettingsCategory.Type.COOKIES) && mRequiresFourStateSetting) {
            setting = cookieSettingsExceptionShouldBlock() ? ContentSettingValues.BLOCK
                                                           : ContentSettingValues.ALLOW;
        } else {
            setting = (WebsitePreferenceBridge.isCategoryEnabled(
                              browserContextHandle, mCategory.getContentSettingsType()))
                    ? ContentSettingValues.BLOCK
                    : ContentSettingValues.ALLOW;
        }

        WebsitePreferenceBridge.setContentSettingForPattern(browserContextHandle,
                mCategory.getContentSettingsType(), primaryPattern, secondaryPattern, setting);

        String hostname = primaryPattern.equals(SITE_WILDCARD) ? secondaryPattern : primaryPattern;
        Toast.makeText(getActivity(),
                     String.format(
                             getActivity().getString(R.string.website_settings_add_site_toast),
                             hostname),
                     Toast.LENGTH_SHORT)
                .show();

        getInfoForOrigins();

        if (mCategory.showSites(SiteSettingsCategory.Type.SOUND)) {
            if (setting == ContentSettingValues.BLOCK) {
                RecordUserAction.record("SoundContentSetting.MuteBy.PatternException");
            } else {
                RecordUserAction.record("SoundContentSetting.UnmuteBy.PatternException");
            }
        }
    }

    /**
     * Reset the preference screen an initialize it again.
     */
    private void resetList() {
        // This will remove the combo box at the top and all the sites listed below it.
        getPreferenceScreen().removeAll();
        // And this will add the filter preference back (combo box).
        SettingsUtils.addPreferencesFromResource(this, R.xml.website_preferences);

        configureGlobalToggles();

        BrowserContextHandle browserContextHandle =
                getSiteSettingsClient().getBrowserContextHandle();
        boolean exception = false;
        if (mCategory.showSites(SiteSettingsCategory.Type.SOUND)) {
            exception = true;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.JAVASCRIPT)) {
            exception = true;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.COOKIES)) {
            exception = true;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.BACKGROUND_SYNC)
                && !WebsitePreferenceBridge.isCategoryEnabled(
                        browserContextHandle, ContentSettingsType.BACKGROUND_SYNC)) {
            exception = true;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS)
                && !WebsitePreferenceBridge.isCategoryEnabled(
                        browserContextHandle, ContentSettingsType.AUTOMATIC_DOWNLOADS)) {
            exception = true;
        }
        if (exception) {
            getPreferenceScreen().addPreference(new AddExceptionPreference(getStyledContext(),
                    ADD_EXCEPTION_KEY, getAddExceptionDialogMessage(), mCategory, this));
        }
    }

    private boolean addWebsites(Collection<Website> sites) {
        filterSelectedDomains(sites);

        List<WebsitePreference> websites = new ArrayList<>();

        // Find origins matching the current search.
        for (Website site : sites) {
            if (mSearch == null || mSearch.isEmpty() || site.getTitle().contains(mSearch)) {
                websites.add(new WebsitePreference(
                        getStyledContext(), getSiteSettingsClient(), site, mCategory));
            }
        }

        mAllowedSiteCount = 0;

        if (websites.size() == 0) {
            updateBlockedHeader(0);
            updateAllowedHeader(0, true);
            updateManagedHeader(0);
            return false;
        }

        Collections.sort(websites);
        int blocked = 0;
        int managed = 0;

        if (!mGroupByAllowBlock) {
            // We're not grouping sites into Allowed/Blocked lists, so show all in order
            // (will be alphabetical).
            for (WebsitePreference website : websites) {
                getPreferenceScreen().addPreference(website);
            }
        } else {
            // Group sites into Allowed/Blocked lists.
            PreferenceGroup allowedGroup =
                    (PreferenceGroup) getPreferenceScreen().findPreference(ALLOWED_GROUP);
            PreferenceGroup blockedGroup =
                    (PreferenceGroup) getPreferenceScreen().findPreference(BLOCKED_GROUP);
            PreferenceGroup managedGroup =
                    (PreferenceGroup) getPreferenceScreen().findPreference(MANAGED_GROUP);

            Set<String> delegatedOrigins =
                    mCategory.showSites(SiteSettingsCategory.Type.NOTIFICATIONS)
                    ? getSiteSettingsClient()
                              .getWebappSettingsClient()
                              .getAllDelegatedNotificationOrigins()
                    : Collections.emptySet();

            for (WebsitePreference website : websites) {
                if (delegatedOrigins.contains(website.site().getAddress().getOrigin())) {
                    managedGroup.addPreference(website);
                    managed += 1;
                } else if (isOnBlockList(website)) {
                    blockedGroup.addPreference(website);
                    blocked += 1;
                } else {
                    allowedGroup.addPreference(website);
                    mAllowedSiteCount += 1;
                }
            }

            // For the ads permission, the Allowed list should appear first. Default
            // collapsed settings should not change.
            if (mCategory.showSites(SiteSettingsCategory.Type.ADS)) {
                blockedGroup.setOrder(allowedGroup.getOrder() + 1);
            }

            // The default, when the lists are shown for the first time, is for the
            // Blocked and Managed list to be collapsed and Allowed expanded -- because
            // the data in the Allowed list is normally more useful than the data in
            // the Blocked/Managed lists. A collapsed initial Blocked/Managed list works
            // well *except* when there's nothing in the Allowed list because then
            // there's only Blocked/Managed items to show and it doesn't make sense for
            // those items to be hidden. So, in those cases (and only when the lists are
            // shown for the first time) do we ignore the collapsed directive. The user
            // can still collapse and expand the Blocked/Managed list at will.
            if (mIsInitialRun) {
                if (mAllowedSiteCount == 0) {
                    if (blocked == 0 && managed > 0) {
                        mManagedListExpanded = true;
                    } else {
                        mBlockListExpanded = true;
                    }
                }
                mIsInitialRun = false;
            }

            if (!mBlockListExpanded) blockedGroup.removeAll();
            if (!mAllowListExpanded) allowedGroup.removeAll();
            if (!mManagedListExpanded) managedGroup.removeAll();
        }

        mWebsites = websites;
        updateBlockedHeader(blocked);
        updateAllowedHeader(mAllowedSiteCount, !isBlocked());
        updateManagedHeader(managed);

        return websites.size() != 0;
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    private void filterSelectedDomains(Collection<Website> websites) {
        if (mSelectedDomains == null) {
            return;
        }
        for (Iterator<Website> it = websites.iterator(); it.hasNext();) {
            String domain =
                    UrlUtilities.getDomainAndRegistry(it.next().getAddress().getOrigin(), true);
            if (!mSelectedDomains.contains(domain)) {
                it.remove();
            }
        }
    }

    private boolean addChosenObjects(Collection<Website> sites) {
        Map<String, Pair<ArrayList<ChosenObjectInfo>, ArrayList<Website>>> objects =
                new HashMap<>();

        // Find chosen objects matching the current search and collect the list of sites
        // that have permission to access each.
        for (Website site : sites) {
            for (ChosenObjectInfo info : site.getChosenObjectInfo()) {
                if (mSearch == null || mSearch.isEmpty()
                        || info.getName().toLowerCase().contains(mSearch)) {
                    Pair<ArrayList<ChosenObjectInfo>, ArrayList<Website>> entry =
                            objects.get(info.getObject());
                    if (entry == null) {
                        entry = Pair.create(
                                new ArrayList<ChosenObjectInfo>(), new ArrayList<Website>());
                        objects.put(info.getObject(), entry);
                    }
                    entry.first.add(info);
                    entry.second.add(site);
                }
            }
        }

        updateBlockedHeader(0);
        updateAllowedHeader(0, true);
        updateManagedHeader(0);

        for (Pair<ArrayList<ChosenObjectInfo>, ArrayList<Website>> entry : objects.values()) {
            Preference preference = new Preference(getStyledContext());
            Bundle extras = preference.getExtras();
            extras.putInt(ChosenObjectSettings.EXTRA_CATEGORY, mCategory.getContentSettingsType());
            extras.putString(EXTRA_TITLE, getActivity().getTitle().toString());
            extras.putSerializable(ChosenObjectSettings.EXTRA_OBJECT_INFOS, entry.first);
            extras.putSerializable(ChosenObjectSettings.EXTRA_SITES, entry.second);
            preference.setIcon(
                    ContentSettingsResources.getIcon(mCategory.getContentSettingsType()));
            preference.setTitle(entry.first.get(0).getName());
            preference.setFragment(ChosenObjectSettings.class.getCanonicalName());
            getPreferenceScreen().addPreference(preference);
        }

        return objects.size() != 0;
    }

    private boolean isBlocked() {
        if (mRequiresTriStateSetting) {
            TriStateSiteSettingsPreference triStateToggle =
                    (TriStateSiteSettingsPreference) getPreferenceScreen().findPreference(
                            TRI_STATE_TOGGLE_KEY);
            return (triStateToggle.getCheckedSetting() == ContentSettingValues.BLOCK);
        } else if (mRequiresFourStateSetting) {
            FourStateCookieSettingsPreference fourStateCookieToggle =
                    (FourStateCookieSettingsPreference) getPreferenceScreen().findPreference(
                            FOUR_STATE_COOKIE_TOGGLE_KEY);
            return fourStateCookieToggle.getState() == CookieSettingsState.BLOCK;
        } else {
            ChromeSwitchPreference binaryToggle =
                    (ChromeSwitchPreference) getPreferenceScreen().findPreference(
                            BINARY_TOGGLE_KEY);
            if (binaryToggle != null) return !binaryToggle.isChecked();
        }
        return false;
    }

    private void configureGlobalToggles() {
        int contentType = mCategory.getContentSettingsType();
        PreferenceScreen screen = getPreferenceScreen();

        // Find all preferences on the current preference screen. Some preferences are
        // not needed for the current category and will be removed in the steps below.
        ChromeSwitchPreference binaryToggle =
                (ChromeSwitchPreference) screen.findPreference(BINARY_TOGGLE_KEY);
        TriStateSiteSettingsPreference triStateToggle =
                (TriStateSiteSettingsPreference) screen.findPreference(TRI_STATE_TOGGLE_KEY);
        FourStateCookieSettingsPreference fourStateCookieToggle =
                (FourStateCookieSettingsPreference) screen.findPreference(
                        FOUR_STATE_COOKIE_TOGGLE_KEY);
        Preference thirdPartyCookies = screen.findPreference(THIRD_PARTY_COOKIES_TOGGLE_KEY);
        Preference notificationsVibrate = screen.findPreference(NOTIFICATIONS_VIBRATE_TOGGLE_KEY);
        Preference notificationsQuietUi = screen.findPreference(NOTIFICATIONS_QUIET_UI_TOGGLE_KEY);
        Preference explainProtectedMediaKey = screen.findPreference(EXPLAIN_PROTECTED_MEDIA_KEY);
        PreferenceGroup allowedGroup = (PreferenceGroup) screen.findPreference(ALLOWED_GROUP);
        PreferenceGroup blockedGroup = (PreferenceGroup) screen.findPreference(BLOCKED_GROUP);
        PreferenceGroup managedGroup = (PreferenceGroup) screen.findPreference(MANAGED_GROUP);
        boolean permissionBlockedByOs = mCategory.showPermissionBlockedMessage(getActivity());
        // For these categories, no binary, tri-state or custom toggles should be shown.
        boolean hideMainToggles = mCategory.showSites(SiteSettingsCategory.Type.ALL_SITES)
                || mCategory.showSites(SiteSettingsCategory.Type.USE_STORAGE);
        boolean hideSecondaryToggles = hideMainToggles || permissionBlockedByOs;

        if (hideMainToggles) {
            screen.removePreference(binaryToggle);
            screen.removePreference(triStateToggle);
            screen.removePreference(fourStateCookieToggle);
        } else if (mRequiresTriStateSetting) {
            screen.removePreference(binaryToggle);
            screen.removePreference(fourStateCookieToggle);
            configureTriStateToggle(triStateToggle, contentType);
        } else if (mRequiresFourStateSetting) {
            screen.removePreference(binaryToggle);
            screen.removePreference(triStateToggle);
            configureFourStateCookieToggle(fourStateCookieToggle);
        } else {
            screen.removePreference(triStateToggle);
            screen.removePreference(fourStateCookieToggle);
            configureBinaryToggle(binaryToggle, contentType);
        }

        if (permissionBlockedByOs) {
            maybeShowOsWarning(screen);
        }

        if (!(mCategory.showSites(SiteSettingsCategory.Type.COOKIES)
                    && ContentSettingsFeatureList.isEnabled(
                            ContentSettingsFeatureList.IMPROVED_COOKIE_CONTROLS))) {
            screen.removePreference(screen.findPreference(COOKIE_INFO_TEXT_KEY));
        }

        if (hideSecondaryToggles) {
            screen.removePreference(thirdPartyCookies);
            screen.removePreference(notificationsVibrate);
            screen.removePreference(notificationsQuietUi);
            screen.removePreference(explainProtectedMediaKey);
            screen.removePreference(allowedGroup);
            screen.removePreference(blockedGroup);
            screen.removePreference(managedGroup);
            // Since all preferences are hidden, there's nothing to do further and we can
            // simply return.
            return;
        }

        // Configure/hide the third-party cookies toggle, as needed.
        if (mCategory.showSites(SiteSettingsCategory.Type.COOKIES) && !mRequiresFourStateSetting) {
            thirdPartyCookies.setOnPreferenceChangeListener(this);
            updateThirdPartyCookiesCheckBox();
        } else {
            screen.removePreference(thirdPartyCookies);
        }

        // Configure/hide the notifications secondary controls, as needed.
        if (mCategory.showSites(SiteSettingsCategory.Type.NOTIFICATIONS)) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
                notificationsVibrate.setOnPreferenceChangeListener(this);
            } else {
                screen.removePreference(notificationsVibrate);
            }

            if (getSiteSettingsClient().isQuietNotificationPromptsFeatureEnabled()) {
                notificationsQuietUi.setOnPreferenceChangeListener(this);
            } else {
                screen.removePreference(notificationsQuietUi);
            }

            updateNotificationsSecondaryControls();
        } else {
            screen.removePreference(notificationsVibrate);
            screen.removePreference(notificationsQuietUi);
        }

        // Only show the link that explains protected content settings when needed.
        if (!mCategory.showSites(SiteSettingsCategory.Type.PROTECTED_MEDIA)) {
            screen.removePreference(explainProtectedMediaKey);
            mListView.setFocusable(true);
        } else {
            // On small screens with no touch input, nested focusable items inside a LinearLayout in
            // ListView cause focus problems when using a keyboard (crbug.com/974413).
            // TODO(chouinard): Verify on a small screen device whether this patch is still needed
            // now that we've migrated this fragment to Support Library (mListView is a RecyclerView
            // now).
            mListView.setFocusable(false);
        }

        // When this menu opens, make sure the Blocked list is collapsed.
        if (!mGroupByAllowBlock) {
            mBlockListExpanded = false;
            mAllowListExpanded = true;
            mManagedListExpanded = false;
        }
        mGroupByAllowBlock = true;

        allowedGroup.setOnPreferenceClickListener(this);
        blockedGroup.setOnPreferenceClickListener(this);
        managedGroup.setOnPreferenceClickListener(this);
    }

    private void maybeShowOsWarning(PreferenceScreen screen) {
        if (isBlocked()) {
            return;
        }

        // Show the link to system settings since permission is disabled.
        ChromeBasePreference osWarning = new ChromeBasePreference(getStyledContext(), null);
        ChromeBasePreference osWarningExtra = new ChromeBasePreference(getStyledContext(), null);
        mCategory.configurePermissionIsOffPreferences(
                osWarning, osWarningExtra, getActivity(), true);
        if (osWarning.getTitle() != null) {
            osWarning.setKey(SingleWebsiteSettings.PREF_OS_PERMISSIONS_WARNING);
            screen.addPreference(osWarning);
        }
        if (osWarningExtra.getTitle() != null) {
            osWarningExtra.setKey(SingleWebsiteSettings.PREF_OS_PERMISSIONS_WARNING_EXTRA);
            screen.addPreference(osWarningExtra);
        }
    }

    private void configureFourStateCookieToggle(
            FourStateCookieSettingsPreference fourStateCookieToggle) {
        fourStateCookieToggle.setOnPreferenceChangeListener(this);
        FourStateCookieSettingsPreference.Params params =
                new FourStateCookieSettingsPreference.Params();
        params.allowCookies = WebsitePreferenceBridge.isCategoryEnabled(
                getSiteSettingsClient().getBrowserContextHandle(), ContentSettingsType.COOKIES);
        params.blockThirdPartyCookies = getPrefs().getBlockThirdPartyCookies();
        params.cookieControlsMode = getPrefs().getCookieControlsMode();
        params.cookiesContentSettingEnforced = mCategory.isManaged();
        params.thirdPartyBlockingEnforced = getPrefs().isBlockThirdPartyCookiesManaged();
        fourStateCookieToggle.setState(params);
    }

    private void configureTriStateToggle(
            TriStateSiteSettingsPreference triStateToggle, int contentType) {
        triStateToggle.setOnPreferenceChangeListener(this);
        @ContentSettingValues
        int setting = WebsitePreferenceBridge.getContentSetting(
                getSiteSettingsClient().getBrowserContextHandle(), contentType);
        int[] descriptionIds =
                ContentSettingsResources.getTriStateSettingDescriptionIDs(contentType);
        triStateToggle.initialize(setting, descriptionIds);
    }

    private void configureBinaryToggle(ChromeSwitchPreference binaryToggle, int contentType) {
        binaryToggle.setOnPreferenceChangeListener(this);
        binaryToggle.setTitle(ContentSettingsResources.getTitle(contentType));

        // Set summary on or off.
        BrowserContextHandle browserContextHandle =
                getSiteSettingsClient().getBrowserContextHandle();
        if (mCategory.showSites(SiteSettingsCategory.Type.DEVICE_LOCATION)
                && WebsitePreferenceBridge.isLocationAllowedByPolicy(browserContextHandle)) {
            binaryToggle.setSummaryOn(ContentSettingsResources.getGeolocationAllowedSummary());
        } else {
            binaryToggle.setSummaryOn(ContentSettingsResources.getEnabledSummary(contentType));
        }
        binaryToggle.setSummaryOff(ContentSettingsResources.getDisabledSummary(contentType));

        binaryToggle.setManagedPreferenceDelegate(new SingleCategoryManagedPreferenceDelegate(
                getSiteSettingsClient().getManagedPreferenceDelegate()));

        // Set the checked value.
        if (mCategory.showSites(SiteSettingsCategory.Type.DEVICE_LOCATION)) {
            binaryToggle.setChecked(
                    WebsitePreferenceBridge.isAllowLocationEnabled(browserContextHandle));
        } else {
            binaryToggle.setChecked(
                    WebsitePreferenceBridge.isCategoryEnabled(browserContextHandle, contentType));
        }
    }

    private void updateThirdPartyCookiesCheckBox() {
        ChromeBaseCheckBoxPreference thirdPartyCookiesPref =
                (ChromeBaseCheckBoxPreference) getPreferenceScreen().findPreference(
                        THIRD_PARTY_COOKIES_TOGGLE_KEY);
        thirdPartyCookiesPref.setChecked(getPrefs().getBlockThirdPartyCookies());
        thirdPartyCookiesPref.setEnabled(WebsitePreferenceBridge.isCategoryEnabled(
                getSiteSettingsClient().getBrowserContextHandle(), ContentSettingsType.COOKIES));
        thirdPartyCookiesPref.setManagedPreferenceDelegate(new ForwardingManagedPreferenceDelegate(
                getSiteSettingsClient().getManagedPreferenceDelegate()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return getPrefs().isBlockThirdPartyCookiesManaged();
            }
        });
    }

    private void updateNotificationsSecondaryControls() {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsClient().getBrowserContextHandle();
        Boolean categoryEnabled = WebsitePreferenceBridge.isCategoryEnabled(
                browserContextHandle, ContentSettingsType.NOTIFICATIONS);

        // The notifications vibrate checkbox.
        ChromeBaseCheckBoxPreference vibrate_pref =
                (ChromeBaseCheckBoxPreference) getPreferenceScreen().findPreference(
                        NOTIFICATIONS_VIBRATE_TOGGLE_KEY);
        if (vibrate_pref != null) vibrate_pref.setEnabled(categoryEnabled);

        if (!getSiteSettingsClient().isQuietNotificationPromptsFeatureEnabled()) return;

        // The notifications quiet ui checkbox.
        ChromeBaseCheckBoxPreference quiet_ui_pref =
                (ChromeBaseCheckBoxPreference) getPreferenceScreen().findPreference(
                        NOTIFICATIONS_QUIET_UI_TOGGLE_KEY);

        if (categoryEnabled) {
            if (quiet_ui_pref == null) {
                getPreferenceScreen().addPreference(mNotificationsQuietUiPref);
                quiet_ui_pref = (ChromeBaseCheckBoxPreference) getPreferenceScreen().findPreference(
                        NOTIFICATIONS_QUIET_UI_TOGGLE_KEY);
            }
            quiet_ui_pref.setChecked(getPrefs().getEnableQuietNotificationPermissionUi());
        } else if (quiet_ui_pref != null) {
            // Save a reference to allow re-adding it to the screen.
            mNotificationsQuietUiPref = quiet_ui_pref;
            getPreferenceScreen().removePreference(quiet_ui_pref);
        }
    }

    private void showManagedToast() {
        if (mCategory.isManagedByCustodian()) {
            ManagedPreferencesUtils.showManagedByParentToast(getActivity(),
                    new SingleCategoryManagedPreferenceDelegate(
                            getSiteSettingsClient().getManagedPreferenceDelegate()));
        } else {
            ManagedPreferencesUtils.showManagedByAdministratorToast(getActivity());
        }
    }

    private SiteSettingsPrefClient getPrefs() {
        return getSiteSettingsClient().getSiteSettingsPrefClient();
    }
}
