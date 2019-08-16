// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_SYSTEM;

import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.util.Pair;
import android.view.View;
import android.widget.RemoteViews;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.BrowserSessionDataProvider;
import org.chromium.chrome.browser.customtabs.dynamicmodule.ModuleMetrics;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.widget.TintedDrawable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;

import androidx.browser.customtabs.CustomTabColorSchemeParams;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.TrustedWebUtils;

/**
 * A model class that parses the incoming intent for Custom Tabs specific customization data.
 *
 * Lifecycle: is activity-scoped, i.e. one instance per CustomTabActivity instance. Must be
 * re-created when color scheme changes, which happens automatically since color scheme change leads
 * to activity re-creation.
 */
public class CustomTabIntentDataProvider extends BrowserSessionDataProvider {
    private static final String TAG = "CustomTabIntentData";

    // The type of UI for Custom Tab to use.
    @IntDef({CustomTabsUiType.DEFAULT, CustomTabsUiType.MEDIA_VIEWER,
            CustomTabsUiType.PAYMENT_REQUEST, CustomTabsUiType.INFO_PAGE,
            CustomTabsUiType.READER_MODE, CustomTabsUiType.MINIMAL_UI_WEBAPP,
            CustomTabsUiType.OFFLINE_PAGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CustomTabsUiType {
        int DEFAULT = 0;
        int MEDIA_VIEWER = 1;
        int PAYMENT_REQUEST = 2;
        int INFO_PAGE = 3;
        int READER_MODE = 4;
        int MINIMAL_UI_WEBAPP = 5;
        int OFFLINE_PAGE = 6;
    }

    @IntDef({LaunchSourceType.OTHER, LaunchSourceType.WEBAPP, LaunchSourceType.WEBAPK,
            LaunchSourceType.MEDIA_LAUNCHER_ACTIVITY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LaunchSourceType {
        int OTHER = -1;
        int WEBAPP = 0;
        int WEBAPK = 1;
        int MEDIA_LAUNCHER_ACTIVITY = 3;
    }

    /**
     * Extra that indicates whether or not the Custom Tab is being launched by an Intent fired by
     * Chrome itself.
     */
    public static final String EXTRA_IS_OPENED_BY_CHROME =
            "org.chromium.chrome.browser.customtabs.IS_OPENED_BY_CHROME";

    /** URL that should be loaded in place of the URL passed along in the data. */
    public static final String EXTRA_MEDIA_VIEWER_URL =
            "org.chromium.chrome.browser.customtabs.MEDIA_VIEWER_URL";

    /** Extra that enables embedded media experience. */
    public static final String EXTRA_ENABLE_EMBEDDED_MEDIA_EXPERIENCE =
            "org.chromium.chrome.browser.customtabs.EXTRA_ENABLE_EMBEDDED_MEDIA_EXPERIENCE";

    /** Indicates the type of UI Custom Tab should use. */
    public static final String EXTRA_UI_TYPE =
            "org.chromium.chrome.browser.customtabs.EXTRA_UI_TYPE";

    /** Extra that defines the initial background color (RGB color stored as an integer). */
    public static final String EXTRA_INITIAL_BACKGROUND_COLOR =
            "org.chromium.chrome.browser.customtabs.EXTRA_INITIAL_BACKGROUND_COLOR";

    /** Extra that enables the client to disable the star button in menu. */
    public static final String EXTRA_DISABLE_STAR_BUTTON =
            "org.chromium.chrome.browser.customtabs.EXTRA_DISABLE_STAR_BUTTON";

    /** Extra that enables the client to disable the download button in menu. */
    public static final String EXTRA_DISABLE_DOWNLOAD_BUTTON =
            "org.chromium.chrome.browser.customtabs.EXTRA_DISABLE_DOWNLOAD_BUTTON";

    /** Extra that indicates whether the client is a WebAPK. */
    public static final String EXTRA_IS_OPENED_BY_WEBAPK =
            "org.chromium.chrome.browser.customtabs.EXTRA_IS_OPENED_BY_WEBAPK";

    /**
     * Indicates the source where the Custom Tab is launched. This is only used for
     * WebApp/WebAPK/TrustedWebActivity. The value is defined as
     * {@link LaunchSourceType}.
     */
    public static final String EXTRA_BROWSER_LAUNCH_SOURCE =
            "org.chromium.chrome.browser.customtabs.EXTRA_BROWSER_LAUNCH_SOURCE";

    // TODO(yusufo): Move this to CustomTabsIntent.
    /** Signals custom tabs to favor sending initial urls to external handler apps if possible. */
    public static final String EXTRA_SEND_TO_EXTERNAL_DEFAULT_HANDLER =
            "android.support.customtabs.extra.SEND_TO_EXTERNAL_HANDLER";

    /** Extra that defines the module managed URLs regex. */
    @VisibleForTesting
    public static final String EXTRA_MODULE_MANAGED_URLS_REGEX =
            "org.chromium.chrome.browser.customtabs.EXTRA_MODULE_MANAGED_URLS_REGEX";

    /** The APK package to load the module from. */
    @VisibleForTesting
    public static final String EXTRA_MODULE_PACKAGE_NAME =
            "org.chromium.chrome.browser.customtabs.EXTRA_MODULE_PACKAGE_NAME";

    /** The asset name of the dex file that contains the module code. */
    private static final String EXTRA_MODULE_DEX_ASSET_NAME =
            "org.chromium.chrome.browser.customtabs.EXTRA_MODULE_DEX_ASSET_NAME";

    /** The class name of the module entry point. */
    @VisibleForTesting
    public static final String EXTRA_MODULE_CLASS_NAME =
            "org.chromium.chrome.browser.customtabs.EXTRA_MODULE_CLASS_NAME";

    /** The custom header's value sent for module managed URLs */
    @VisibleForTesting
    public static final String EXTRA_MODULE_MANAGED_URLS_HEADER_VALUE =
            "org.chromium.chrome.browser.customtabs.EXTRA_MODULE_MANAGED_URLS_HEADER_VALUE";

    /** Extra that indicates whether to hide the CCT header on module managed URLs. */
    @VisibleForTesting
    public static final String EXTRA_HIDE_CCT_HEADER_ON_MODULE_MANAGED_URLS =
            "org.chromium.chrome.browser.customtabs.EXTRA_HIDE_CCT_HEADER_ON_MODULE_MANAGED_URLS";

    // TODO(amalova): Move this to CustomTabsIntent.
    /**
     * Extra that, if set, specifies Translate UI should be triggered with
     * specified target language.
     */
    private static final String EXTRA_TRANSLATE_LANGUAGE =
            "androidx.browser.customtabs.extra.TRANSLATE_LANGUAGE";

    private static final int MAX_CUSTOM_MENU_ITEMS = 5;

    private static final int MAX_CUSTOM_TOOLBAR_ITEMS = 2;

    private static final String FIRST_PARTY_PITFALL_MSG =
            "The intent contains a non-default UI type, but it is not from a first-party app. "
            + "To make locally-built Chrome a first-party app, sign with release-test "
            + "signing keys and run on userdebug devices. See use_signing_keys GN arg.";

    private final Intent mIntent;

    private final int mUiType;
    private final int mTitleVisibilityState;
    private final String mMediaViewerUrl;
    private final boolean mEnableEmbeddedMediaExperience;
    private final boolean mIsFromMediaLauncherActivity;
    private final int mInitialBackgroundColor;
    private final boolean mDisableStar;
    private final boolean mDisableDownload;
    private final boolean mIsOpenedByWebApk;
    private final boolean mIsTrustedWebActivity;
    @Nullable
    private final Integer mNavigationBarColor;
    @Nullable
    private final ComponentName mModuleComponentName;
    @Nullable
    private final Pattern mModuleManagedUrlsPattern;
    @Nullable
    private final String mModuleManagedUrlsHeaderValue;
    private final boolean mHideCctHeaderOnModuleManagedUrls;
    @Nullable
    private final String mModuleDexAssetName;
    private final boolean mIsIncognito;
    @Nullable
    private final List<String> mTrustedWebActivityAdditionalOrigins;
    @Nullable
    private String mUrlToLoad;

    private int mToolbarColor;
    private int mBottomBarColor;
    private boolean mEnableUrlBarHiding;
    private List<CustomButtonParams> mCustomButtonParams;
    private Drawable mCloseButtonIcon;
    private List<Pair<String, PendingIntent>> mMenuEntries = new ArrayList<>();
    private boolean mShowShareItem;
    private List<CustomButtonParams> mToolbarButtons = new ArrayList<>(1);
    private List<CustomButtonParams> mBottombarButtons = new ArrayList<>(2);
    private RemoteViews mRemoteViews;
    private int[] mClickableViewIds;
    private PendingIntent mRemoteViewsPendingIntent;
    // OnFinished listener for PendingIntents. Used for testing only.
    private PendingIntent.OnFinished mOnFinished;

    /** Whether this CustomTabActivity was explicitly started by another Chrome Activity. */
    private final boolean mIsOpenedByChrome;

    /** ISO 639 language code */
    @Nullable
    private final String mTranslateLanguage;

    /**
     * Add extras to customize menu items for opening payment request UI custom tab from Chrome.
     */
    public static void addPaymentRequestUIExtras(Intent intent) {
        intent.putExtra(EXTRA_UI_TYPE, CustomTabsUiType.PAYMENT_REQUEST);
        intent.putExtra(EXTRA_IS_OPENED_BY_CHROME, true);
        IntentHandler.addTrustedIntentExtras(intent);
    }

    /**
     * Add extras to customize menu items for opening Reader Mode UI custom tab from Chrome.
     */
    public static void addReaderModeUIExtras(Intent intent) {
        intent.putExtra(EXTRA_UI_TYPE, CustomTabsUiType.READER_MODE);
        intent.putExtra(EXTRA_IS_OPENED_BY_CHROME, true);
        IntentHandler.addTrustedIntentExtras(intent);
    }

    /**
     * Constructs a {@link CustomTabIntentDataProvider}.
     *
     * The colorScheme parameter specifies which color scheme the Custom Tab should use.
     * It can currently be either {@link CustomTabsIntent#COLOR_SCHEME_LIGHT} or
     * {@link CustomTabsIntent#COLOR_SCHEME_DARK}.
     * If Custom Tab was launched with {@link CustomTabsIntent#COLOR_SCHEME_SYSTEM}, colorScheme
     * must reflect the current system setting. When the system setting changes, a new
     * CustomTabIntentDataProvider object must be created.
     */
    public CustomTabIntentDataProvider(Intent intent, Context context, int colorScheme) {
        super(intent);
        if (intent == null) assert false;

        mIntent = intent;
        mIsOpenedByChrome = IntentUtils.safeGetBooleanExtra(
                intent, EXTRA_IS_OPENED_BY_CHROME, false);

        final int requestedUiType =
                IntentUtils.safeGetIntExtra(intent, EXTRA_UI_TYPE, CustomTabsUiType.DEFAULT);
        mUiType = verifiedUiType(requestedUiType);

        mIsIncognito = isIncognitoForPaymentsFlow(intent) || isValidExternalIncognitoIntent(intent);

        CustomTabColorSchemeParams params = getColorSchemeParams(intent, colorScheme);
        retrieveCustomButtons(intent, context);
        retrieveToolbarColor(params, context);
        retrieveBottomBarColor(params);
        mNavigationBarColor = params.navigationBarColor == null
                ? null
                : ColorUtils.getOpaqueColor(params.navigationBarColor);
        mInitialBackgroundColor = retrieveInitialBackgroundColor(intent);

        mEnableUrlBarHiding = IntentUtils.safeGetBooleanExtra(
                intent, CustomTabsIntent.EXTRA_ENABLE_URLBAR_HIDING, true);

        Bitmap bitmap = IntentUtils.safeGetParcelableExtra(
                intent, CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON);
        if (bitmap != null && !checkCloseButtonSize(context, bitmap)) {
            IntentUtils.safeRemoveExtra(intent, CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON);
            bitmap.recycle();
            bitmap = null;
        }
        if (bitmap == null) {
            mCloseButtonIcon =
                    TintedDrawable.constructTintedDrawable(context, R.drawable.btn_close);
        } else {
            mCloseButtonIcon = new BitmapDrawable(context.getResources(), bitmap);
        }

        List<Bundle> menuItems =
                IntentUtils.getParcelableArrayListExtra(intent, CustomTabsIntent.EXTRA_MENU_ITEMS);
        if (menuItems != null) {
            for (int i = 0; i < Math.min(MAX_CUSTOM_MENU_ITEMS, menuItems.size()); i++) {
                Bundle bundle = menuItems.get(i);
                String title =
                        IntentUtils.safeGetString(bundle, CustomTabsIntent.KEY_MENU_ITEM_TITLE);
                PendingIntent pendingIntent =
                        IntentUtils.safeGetParcelable(bundle, CustomTabsIntent.KEY_PENDING_INTENT);
                if (TextUtils.isEmpty(title) || pendingIntent == null) continue;
                mMenuEntries.add(new Pair<String, PendingIntent>(title, pendingIntent));
            }
        }

        mIsTrustedWebActivity = IntentUtils.safeGetBooleanExtra(
                intent, TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false);
        mTrustedWebActivityAdditionalOrigins = IntentUtils.safeGetStringArrayListExtra(intent,
                TrustedWebUtils.EXTRA_ADDITIONAL_TRUSTED_ORIGINS);
        mTitleVisibilityState = IntentUtils.safeGetIntExtra(
                intent, CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE, CustomTabsIntent.NO_TITLE);
        mShowShareItem = IntentUtils.safeGetBooleanExtra(intent,
                CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM,
                mIsOpenedByChrome && mUiType == CustomTabsUiType.DEFAULT);
        mRemoteViews =
                IntentUtils.safeGetParcelableExtra(intent, CustomTabsIntent.EXTRA_REMOTEVIEWS);
        mClickableViewIds = IntentUtils.safeGetIntArrayExtra(
                intent, CustomTabsIntent.EXTRA_REMOTEVIEWS_VIEW_IDS);
        mRemoteViewsPendingIntent = IntentUtils.safeGetParcelableExtra(
                intent, CustomTabsIntent.EXTRA_REMOTEVIEWS_PENDINGINTENT);
        mMediaViewerUrl = isMediaViewer()
                ? IntentUtils.safeGetStringExtra(intent, EXTRA_MEDIA_VIEWER_URL)
                : null;
        mEnableEmbeddedMediaExperience = isTrustedIntent()
                && IntentUtils.safeGetBooleanExtra(
                           intent, EXTRA_ENABLE_EMBEDDED_MEDIA_EXPERIENCE, false);
        mIsFromMediaLauncherActivity = isTrustedIntent()
                && (IntentUtils.safeGetIntExtra(
                            intent, EXTRA_BROWSER_LAUNCH_SOURCE, LaunchSourceType.OTHER)
                           == LaunchSourceType.MEDIA_LAUNCHER_ACTIVITY);
        mDisableStar = IntentUtils.safeGetBooleanExtra(intent, EXTRA_DISABLE_STAR_BUTTON, false);
        mDisableDownload =
                IntentUtils.safeGetBooleanExtra(intent, EXTRA_DISABLE_DOWNLOAD_BUTTON, false);
        mIsOpenedByWebApk =
                IntentUtils.safeGetBooleanExtra(intent, EXTRA_IS_OPENED_BY_WEBAPK, false);

        mTranslateLanguage = IntentUtils.safeGetStringExtra(intent, EXTRA_TRANSLATE_LANGUAGE);

        String modulePackageName =
                IntentUtils.safeGetStringExtra(intent, EXTRA_MODULE_PACKAGE_NAME);
        String moduleClassName = IntentUtils.safeGetStringExtra(intent, EXTRA_MODULE_CLASS_NAME);
        if (modulePackageName != null && moduleClassName != null) {
            mModuleComponentName = new ComponentName(modulePackageName, moduleClassName);
            mModuleDexAssetName =
                    IntentUtils.safeGetStringExtra(intent, EXTRA_MODULE_DEX_ASSET_NAME);
            String moduleManagedUrlsRegex =
                    IntentUtils.safeGetStringExtra(intent, EXTRA_MODULE_MANAGED_URLS_REGEX);
            mModuleManagedUrlsPattern = (moduleManagedUrlsRegex != null)
                    ? Pattern.compile(moduleManagedUrlsRegex)
                    : null;
            mModuleManagedUrlsHeaderValue =
                    IntentUtils.safeGetStringExtra(intent, EXTRA_MODULE_MANAGED_URLS_HEADER_VALUE);
            mHideCctHeaderOnModuleManagedUrls = IntentUtils.safeGetBooleanExtra(
                    intent, EXTRA_HIDE_CCT_HEADER_ON_MODULE_MANAGED_URLS, false);
        } else {
            mModuleComponentName = null;
            mModuleManagedUrlsPattern = null;
            mModuleManagedUrlsHeaderValue = null;
            mHideCctHeaderOnModuleManagedUrls = false;
            mModuleDexAssetName = null;
        }
    }

    @NonNull
    private CustomTabColorSchemeParams getColorSchemeParams(Intent intent, int colorScheme) {
        if (colorScheme == COLOR_SCHEME_SYSTEM) {
            assert false : "Color scheme passed to IntentDataProvider should not be "
                    + "COLOR_SCHEME_SYSTEM";
            colorScheme = COLOR_SCHEME_LIGHT;
        }
        try {
            return CustomTabsIntent.getColorSchemeParams(intent, colorScheme);
        } catch (Throwable e) {
            // Catch any un-parceling exceptions, like in IntentUtils#safe* methods
            Log.e(TAG, "Failed to parse CustomTabColorSchemeParams");
            return new CustomTabColorSchemeParams.Builder().build(); // Empty params
        }
    }

    private boolean isIncognitoForPaymentsFlow(Intent intent) {
        return incognitoRequested(intent) && isTrustedIntent() && isOpenedByChrome()
                && isForPaymentRequest();
    }

    public static boolean isValidExternalIncognitoIntent(Intent intent) {
        if (!CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_INCOGNITO_CUSTOM_TABS)) {
            return false;
        }

        if (!incognitoRequested(intent)) {
            return false;
        }

        return isVerifiedFirstPartyIntent(intent)
                || CommandLine.getInstance().hasSwitch(
                           ChromeSwitches.ALLOW_INCOGNITO_CUSTOM_TABS_FROM_THIRD_PARTY);
    }

    private static boolean incognitoRequested(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(
                intent, IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);
    }

    private static boolean isVerifiedFirstPartyIntent(Intent intent) {
        CustomTabsSessionToken sessionToken =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        String packageNameFromSession =
                CustomTabsConnection.getInstance().getClientPackageNameForSession(sessionToken);
        return !TextUtils.isEmpty(packageNameFromSession)
                && ExternalAuthUtils.getInstance().isGoogleSigned(packageNameFromSession);
    }

    /**
     * Get the verified UI type, according to the intent extras, and whether the intent is trusted.
     * @param requestedUiType requested UI type in the intent, unqualified
     * @return verified UI type
     */
    private int verifiedUiType(int requestedUiType) {
        if (!isTrustedIntent()) {
            if (ChromeVersionInfo.isLocalBuild()) Log.w(TAG, FIRST_PARTY_PITFALL_MSG);
            return CustomTabsUiType.DEFAULT;
        }

        if (requestedUiType == CustomTabsUiType.PAYMENT_REQUEST) {
            if (!mIsOpenedByChrome) {
                return CustomTabsUiType.DEFAULT;
            }
        }

        return requestedUiType;
    }

    /**
     * Gets custom buttons from the intent and updates {@link #mCustomButtonParams},
     * {@link #mBottombarButtons} and {@link #mToolbarButtons}.
     */
    private void retrieveCustomButtons(Intent intent, Context context) {
        assert mCustomButtonParams == null;
        mCustomButtonParams = CustomButtonParams.fromIntent(context, intent);
        for (CustomButtonParams params : mCustomButtonParams) {
            if (!params.showOnToolbar()) {
                mBottombarButtons.add(params);
            } else if (mToolbarButtons.size() < getMaxCustomToolbarItems()) {
                mToolbarButtons.add(params);
            } else {
                Log.w(TAG, "Only %d items are allowed in the toolbar", getMaxCustomToolbarItems());
            }
        }
    }

    private int getMaxCustomToolbarItems() {
        if (!isTrustedIntent()) return 1;

        return MAX_CUSTOM_TOOLBAR_ITEMS;
    }

    /**
     * Processes the color passed from the client app and updates {@link #mToolbarColor}.
     */
    private void retrieveToolbarColor(CustomTabColorSchemeParams schemeParams, Context context) {
        int defaultColor = ColorUtils.getDefaultThemeColor(context.getResources(), isIncognito());
        if (isIncognito()) {
            mToolbarColor = defaultColor;
            return; // Don't allow toolbar color customization for incognito tabs.
        }
        int color = schemeParams.toolbarColor != null ? schemeParams.toolbarColor : defaultColor;
        mToolbarColor = ColorUtils.getOpaqueColor(color);
    }

    /**
     * Must be called after calling {@link #retrieveToolbarColor}.
     */
    private void retrieveBottomBarColor(CustomTabColorSchemeParams schemeParams) {
        if (isIncognito()) {
            mBottomBarColor = mToolbarColor;
            return;
        }
        int defaultColor = mToolbarColor;
        int color = schemeParams.secondaryToolbarColor != null ? schemeParams.secondaryToolbarColor
                : defaultColor;
        mBottomBarColor = ColorUtils.getOpaqueColor(color);
    }

    /**
     * Returns the color to initialize the background of the Custom Tab with.
     * If no valid color is set, Color.TRANSPARENT is returned.
     */
    private int retrieveInitialBackgroundColor(Intent intent) {
        int defaultColor = Color.TRANSPARENT;
        int color =
                IntentUtils.safeGetIntExtra(intent, EXTRA_INITIAL_BACKGROUND_COLOR, defaultColor);
        return color == Color.TRANSPARENT ? color : ColorUtils.getOpaqueColor(color);
    }

    private String resolveUrlToLoad(Intent intent) {
        String url = IntentHandler.getUrlFromIntent(intent);

        // Intents fired for media viewers have an additional file:// URI passed along so that the
        // tab can display the actual filename to the user when it is loaded.
        if (isMediaViewer()) {
            String mediaViewerUrl = getMediaViewerUrl();
            if (!TextUtils.isEmpty(mediaViewerUrl)) {
                Uri mediaViewerUri = Uri.parse(mediaViewerUrl);
                if (UrlConstants.FILE_SCHEME.equals(mediaViewerUri.getScheme())) {
                    url = mediaViewerUrl;
                }
            }
        }

        if (!TextUtils.isEmpty(url)) {
            url = DataReductionProxySettings.getInstance().maybeRewriteWebliteUrl(url);
        }

        return url;
    }

    /**
     * @return The URL that should be used from this intent. If it is a WebLite url, it may be
     *         overridden if the Data Reduction Proxy is using Lo-Fi previews.
     * Must be called only after native has loaded.
     */
    public String getUrlToLoad() {
        if (mUrlToLoad == null) {
            mUrlToLoad = resolveUrlToLoad(mIntent);
        }
        return mUrlToLoad;
    }

    /**
     * @return Whether url bar hiding should be enabled in the custom tab. Default is false.
     * It should be impossible to hide the url bar when the tab is opened for Payment Request.
     */
    public boolean shouldEnableUrlBarHiding() {
        return mEnableUrlBarHiding && !isForPaymentRequest();
    }

    /**
     * @return The toolbar color specified in the intent. Will return the  default theme color, if
     *         not set in the intent.
     */
    public int getToolbarColor() {
        return mToolbarColor;
    }

    /**
     * @return The navigation bar color specified in the intent, or null if not specified.
     */
    @Nullable
    public Integer getNavigationBarColor() {
        return mNavigationBarColor;
    }

    /**
     * @return The drawable of the icon of close button shown in the custom tab toolbar. If the
     *         client app provides an icon in valid size, use this icon; else return the default
     *         drawable.
     */
    public Drawable getCloseButtonDrawable() {
        return mCloseButtonIcon;
    }

    /**
     * @return The title visibility state for the toolbar.
     *         Default is {@link CustomTabsIntent#NO_TITLE}.
     */
    public int getTitleVisibilityState() {
        return mTitleVisibilityState;
    }

    /**
     * @return Whether the default share item should be shown in the menu.
     */
    public boolean shouldShowShareMenuItem() {
        return mShowShareItem;
    }

    /**
     * @return The params for the custom buttons that show on the toolbar.
     */
    public List<CustomButtonParams> getCustomButtonsOnToolbar() {
        return mToolbarButtons;
    }

    /**
     * @return The list of params representing the buttons on the bottombar.
     */
    public List<CustomButtonParams> getCustomButtonsOnBottombar() {
        return mBottombarButtons;
    }

    /**
     * @return Whether the bottom bar should be shown.
     */
    public boolean shouldShowBottomBar() {
        return !mBottombarButtons.isEmpty() || mRemoteViews != null;
    }

    /**
     * @return The color of the bottom bar, or {@link #getToolbarColor()} if not specified.
     */
    public int getBottomBarColor() {
        return mBottomBarColor;
    }

    /**
     * @return The {@link RemoteViews} to show on the bottom bar, or null if the extra is not
     *         specified.
     */
    public RemoteViews getBottomBarRemoteViews() {
        return mRemoteViews;
    }

    /**
     * @return A array of {@link View} ids, of which the onClick event is handled by the custom tab.
     */
    public int[] getClickableViewIDs() {
        if (mClickableViewIds == null) return null;
        return mClickableViewIds.clone();
    }

    /**
     * @return The {@link PendingIntent} that is sent when the user clicks on the remote view.
     */
    public PendingIntent getRemoteViewsPendingIntent() {
        return mRemoteViewsPendingIntent;
    }

    /**
     * Gets params for all custom buttons, which is the combination of
     * {@link #getCustomButtonsOnBottombar()} and {@link #getCustomButtonsOnToolbar()}.
     */
    public List<CustomButtonParams> getAllCustomButtons() {
        return mCustomButtonParams;
    }

    /**
     * Searches for the toolbar button with the given {@code id} and returns its index.
     * @param id The ID of a toolbar button to search for.
     * @return The index of the toolbar button with the given {@code id}, or -1 if no such button
     *         can be found.
     */
    public int getCustomToolbarButtonIndexForId(int id) {
        for (int i = 0; i < mToolbarButtons.size(); i++) {
            if (mToolbarButtons.get(i).getId() == id) return i;
        }
        return -1;
    }

    /**
     * @return The {@link CustomButtonParams} (either on the toolbar or bottom bar) with the given
     *         {@code id}, or null if no such button can be found.
     */
    public CustomButtonParams getButtonParamsForId(int id) {
        for (CustomButtonParams params : mCustomButtonParams) {
            // A custom button params will always carry an ID. If the client calls updateVisuals()
            // without an id, we will assign the toolbar action button id to it.
            if (id == params.getId()) return params;
        }
        return null;
    }

    /**
     * @return Titles of menu items that were passed from client app via intent.
     */
    public List<String> getMenuTitles() {
        ArrayList<String> list = new ArrayList<>();
        for (Pair<String, PendingIntent> pair : mMenuEntries) {
            list.add(pair.first);
        }
        return list;
    }

    /**
     * Triggers the client-defined action when the user clicks a custom menu item.
     * @param activity The {@link ChromeActivity} to use for sending the {@link PendingIntent}.
     * @param menuIndex The index that the menu item is shown in the result of
     *                  {@link #getMenuTitles()}.
     * @param url The URL to attach as additional data to the {@link PendingIntent}.
     * @param title The title to attach as additional data to the {@link PendingIntent}.
     */
    public void clickMenuItemWithUrlAndTitle(
            ChromeActivity activity, int menuIndex, String url, String title) {
        Intent addedIntent = new Intent();
        addedIntent.setData(Uri.parse(url));
        addedIntent.putExtra(Intent.EXTRA_SUBJECT, title);
        try {
            // Media viewers pass in PendingIntents that contain CHOOSER Intents.  Setting the data
            // in these cases prevents the Intent from firing correctly.
            String menuTitle = mMenuEntries.get(menuIndex).first;
            PendingIntent pendingIntent = mMenuEntries.get(menuIndex).second;
            pendingIntent.send(
                    activity, 0, isMediaViewer() ? null : addedIntent, mOnFinished, null);
            if (shouldEnableEmbeddedMediaExperience()
                    && TextUtils.equals(menuTitle,
                               activity.getString(R.string.download_manager_open_with))) {
                RecordUserAction.record("CustomTabsMenuCustomMenuItem.DownloadsUI.OpenWith");
            }
        } catch (CanceledException e) {
            Log.e(TAG, "Custom tab in Chrome failed to send pending intent.");
        }
    }

    /**
     * Sends the pending intent for the custom button on the toolbar with the given {@code params},
     *         with the given {@code url} as data.
     * @param context The context to use for sending the {@link PendingIntent}.
     * @param params The parameters for the custom button.
     * @param url The URL to attach as additional data to the {@link PendingIntent}.
     * @param title The title to attach as additional data to the {@link PendingIntent}.
     */
    public void sendButtonPendingIntentWithUrlAndTitle(
            Context context, CustomButtonParams params, String url, String title) {
        Intent addedIntent = new Intent();
        addedIntent.setData(Uri.parse(url));
        addedIntent.putExtra(Intent.EXTRA_SUBJECT, title);
        try {
            params.getPendingIntent().send(context, 0, addedIntent, mOnFinished, null);
        } catch (CanceledException e) {
            Log.e(TAG, "CanceledException while sending pending intent in custom tab");
        }
    }

    private boolean checkCloseButtonSize(Context context, Bitmap bitmap) {
        int size = context.getResources().getDimensionPixelSize(R.dimen.toolbar_icon_height);
        if (bitmap.getHeight() == size && bitmap.getWidth() == size) return true;
        return false;
    }

    /**
     * Set the callback object for {@link PendingIntent}s that are sent in this class. For testing
     * purpose only.
     */
    @VisibleForTesting
    void setPendingIntentOnFinishedForTesting(PendingIntent.OnFinished onFinished) {
        mOnFinished = onFinished;
    }

    /**
     * @return See {@link #EXTRA_IS_OPENED_BY_CHROME}.
     */
    public boolean isOpenedByChrome() {
        return mIsOpenedByChrome;
    }

    /**
     * @return See {@link #EXTRA_UI_TYPE}.
     */
    public boolean isMediaViewer() {
        return mUiType == CustomTabsUiType.MEDIA_VIEWER;
    }

    @CustomTabsUiType
    int getUiType() {
        return mUiType;
    }

    /**
     * @return See {@link #EXTRA_MEDIA_VIEWER_URL}.
     */
    String getMediaViewerUrl() {
        return mMediaViewerUrl;
    }

    /**
     * @return See {@link #EXTRA_ENABLE_EMBEDDED_MEDIA_EXPERIENCE}
     */
    public boolean shouldEnableEmbeddedMediaExperience() {
        return mEnableEmbeddedMediaExperience;
    }

    /**
     * @return See {@link #EXTRA_IS_FROM_MEDIA_LAUNCHER_ACTIVITY}
     */
    boolean isFromMediaLauncherActivity() {
        return mIsFromMediaLauncherActivity;
    }

    /**
     * @return If the Custom Tab is an info page.
     * See {@link #EXTRA_UI_TYPE}.
     */
    public boolean isInfoPage() {
        return mUiType == CustomTabsUiType.INFO_PAGE;
    }

    /**
     * See {@link #EXTRA_INITIAL_BACKGROUND_COLOR}.
     * @return The color if it was specified in the Intent, Color.TRANSPARENT otherwise.
     */
    public int getInitialBackgroundColor() {
        return mInitialBackgroundColor;
    }

    /**
     * @return Whether there should be a star button in the menu.
     */
    boolean shouldShowStarButton() {
        return !mDisableStar;
    }

    /**
     * @return Whether there should be a download button in the menu.
     */
    boolean shouldShowDownloadButton() {
        return !mDisableDownload;
    }

    /**
     * @return Whether the Custom Tab was opened from a WebAPK.
     */
    public boolean isOpenedByWebApk() {
        return mIsOpenedByWebApk;
    }

    /**
     * @return Whether the Custom Tab is opened for payment request.
     */
    boolean isForPaymentRequest() {
        return mUiType == CustomTabsUiType.PAYMENT_REQUEST;
    }

    /**
     * @return Whether the custom Tab should be opened in incognito mode.
     */
    public boolean isIncognito() {
        return mIsIncognito;
    }

    /**
     * @return Whether the Custom Tab should attempt to display a Trusted Web Activity.
     */
    boolean isTrustedWebActivity() {
        return mIsTrustedWebActivity;
    }

    /**
     * @return Whether the Custom Tab should attempt to load a dynamic module, i.e.
     * if the feature is enabled, the package is provided and package is Google-signed.
     *
     * Will return false if native is not initialized.
     */
    public boolean isDynamicModuleEnabled() {
        if (!ChromeFeatureList.isInitialized()) return false;

        ComponentName componentName = getModuleComponentName();
        // Return early if no component name was provided. It's important to do this before checking
        // the feature experiment group, to avoid entering users into the experiment that do not
        // even receive the extras for using the feature.
        if (componentName == null) return false;

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_MODULE)) {
            Log.w(TAG, "The %s feature is disabled.", ChromeFeatureList.CCT_MODULE);
            ModuleMetrics.recordLoadResult(ModuleMetrics.LoadResult.FEATURE_DISABLED);
            return false;
        }

        ExternalAuthUtils authUtils = ChromeApplication.getComponent().resolveExternalAuthUtils();
        if (!authUtils.isGoogleSigned(componentName.getPackageName())) {
            Log.w(TAG, "The %s package is not Google-signed.", componentName.getPackageName());
            ModuleMetrics.recordLoadResult(ModuleMetrics.LoadResult.NOT_GOOGLE_SIGNED);
            return false;
        }

        return true;
    }

    /**
     * @return The component name of the module entry point, or null if not specified.
     */
    @Nullable
    public ComponentName getModuleComponentName() {
        return mModuleComponentName;
    }

    /**
     * @return The resource identifier for the dex that contains module code. {@code 0} if no dex
     * resource is provided.
     */
    @Nullable
    public String getModuleDexAssetName() {
        return mModuleDexAssetName;
    }

    /**
     * See {@link #EXTRA_MODULE_MANAGED_URLS_REGEX}.
     * @return The pattern compiled from the regex that defines the module managed URLs,
     * or null if not specified.
     */
    @Nullable
    public Pattern getExtraModuleManagedUrlsPattern() {
        return mModuleManagedUrlsPattern;
    }

    /**
     * See {@link #EXTRA_MODULE_MANAGED_URLS_HEADER_VALUE}.
     * @return The header value sent to managed hosts when the URL matches the
     *         EXTRA_MODULE_MANAGED_URLS_REGEX.
     */
    @Nullable
    public String getExtraModuleManagedUrlsHeaderValue() {
        return mModuleManagedUrlsHeaderValue;
    }

    /**
     * @return the Intent this instance was created with.
     */
    public Intent getIntent() {
        return mIntent;
    }

    /**
     * @return Whether to hide CCT header on module managed URLs.
     */
    public boolean shouldHideCctHeaderOnModuleManagedUrls() {
        return mHideCctHeaderOnModuleManagedUrls;
    }

    /**
     * @return Additional origins associated with a Trusted Web Activity client app.
     */
    @Nullable
    public List<String> getTrustedWebActivityAdditionalOrigins() {
        return mTrustedWebActivityAdditionalOrigins;
    }

    /**
     * @return ISO 639 code of target language the page should be translated to.
     * This method requires native.
     */
    @Nullable
    public String getTranslateLanguage() {
        boolean isEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_TARGET_TRANSLATE_LANGUAGE);
        return isEnabled ? mTranslateLanguage : null;
    }
}
