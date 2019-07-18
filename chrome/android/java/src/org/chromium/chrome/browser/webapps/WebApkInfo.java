// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.IntDef;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.webapk.lib.common.WebApkCommonUtils;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.lib.common.WebApkMetaDataUtils;
import org.chromium.webapk.lib.common.splash.SplashLayout;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Stores info for WebAPK.
 */
public class WebApkInfo extends WebappInfo {
    // A class that stores share information from share intent.
    protected static class ShareData {
        public String subject;
        public String text;
        public ArrayList<Uri> files;
        public String shareActivityClassName;
    }

    /**
     * Stores information about the WebAPK's share intent handlers.
     * TODO(crbug.com/912954): add share target V2 parameters once the server supports them.
     */
    public static class ShareTarget {
        private static final int ACTION_INDEX = 0;
        private static final int PARAM_TITLE_INDEX = 1;
        private static final int PARAM_TEXT_INDEX = 2;
        private static final int PARAM_URL_INDEX = 3;
        private String[] mData;
        private boolean mIsShareMethodPost;
        private boolean mIsShareEncTypeMultipart;
        private String[] mFileNames;
        private String[][] mFileAccepts;

        public ShareTarget() {
            this(null, null, null, null, false, false, null, null);
        }

        public ShareTarget(String action, String paramTitle, String paramText, String paramUrl,
                boolean isMethodPost, boolean isEncTypeMultipart, String[] fileNames,
                String[][] fileAccepts) {
            mData = new String[4];
            mData[ACTION_INDEX] = replaceNullWithEmpty(action);
            mData[PARAM_TITLE_INDEX] = replaceNullWithEmpty(paramTitle);
            mData[PARAM_TEXT_INDEX] = replaceNullWithEmpty(paramText);
            mData[PARAM_URL_INDEX] = replaceNullWithEmpty(paramUrl);
            mIsShareMethodPost = isMethodPost;
            mIsShareEncTypeMultipart = isEncTypeMultipart;

            mFileNames = fileNames != null ? fileNames : new String[0];
            mFileAccepts = fileAccepts != null ? fileAccepts : new String[0][];
        }

        @Override
        public boolean equals(Object o) {
            if (!(o instanceof ShareTarget)) return false;
            ShareTarget shareTarget = (ShareTarget) o;
            return Arrays.equals(mData, shareTarget.mData)
                    && mIsShareMethodPost == shareTarget.mIsShareMethodPost
                    && mIsShareEncTypeMultipart == shareTarget.mIsShareEncTypeMultipart
                    && Arrays.equals(mFileNames, shareTarget.mFileNames)
                    && Arrays.deepEquals(mFileAccepts, shareTarget.mFileAccepts);
        }

        public String getAction() {
            return mData[ACTION_INDEX];
        }

        public String getParamTitle() {
            return mData[PARAM_TITLE_INDEX];
        }

        public String getParamText() {
            return mData[PARAM_TEXT_INDEX];
        }

        public String getParamUrl() {
            return mData[PARAM_URL_INDEX];
        }

        public boolean isShareMethodPost() {
            return mIsShareMethodPost;
        }

        public boolean isShareEncTypeMultipart() {
            return mIsShareEncTypeMultipart;
        }

        public String[] getFileNames() {
            return mFileNames;
        }

        public String[][] getFileAccepts() {
            return mFileAccepts;
        }
    }

    public static final String RESOURCE_NAME = "name";
    public static final String RESOURCE_SHORT_NAME = "short_name";
    public static final String RESOURCE_STRING_TYPE = "string";

    @IntDef({WebApkDistributor.BROWSER, WebApkDistributor.DEVICE_POLICY, WebApkDistributor.OTHER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface WebApkDistributor {
        int BROWSER = 0;
        int DEVICE_POLICY = 1;
        int OTHER = 2;
    }

    private static final String TAG = "WebApkInfo";

    private Icon mBadgeIcon;
    private Icon mSplashIcon;
    private String mApkPackageName;
    private int mShellApkVersion;
    private String mManifestUrl;
    private String mManifestStartUrl;
    private @WebApkDistributor int mDistributor;
    private ShareTarget mShareTarget;
    private String mShareTargetActivityName;
    private Map<String, String> mIconUrlToMurmur2HashMap;
    private boolean mIsSplashProvidedByWebApk;

    private ShareData mShareData;

    public static WebApkInfo createEmpty() {
        return new WebApkInfo();
    }

    /**
     * Constructs a WebApkInfo from the passed in Intent and <meta-data> in the WebAPK's Android
     * manifest.
     * @param intent Intent containing info about the app.
     */
    public static WebApkInfo create(Intent intent) {
        String webApkPackageName =
                IntentUtils.safeGetStringExtra(intent, WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME);

        if (TextUtils.isEmpty(webApkPackageName)) {
            return null;
        }

        // Force navigation if the extra is not specified to avoid breaking deep linking for old
        // WebAPKs which don't specify the {@link ShortcutHelper#EXTRA_FORCE_NAVIGATION} intent
        // extra.
        boolean forceNavigation = IntentUtils.safeGetBooleanExtra(
                intent, ShortcutHelper.EXTRA_FORCE_NAVIGATION, true);

        ShareData shareData = null;

        String shareActivityClassName = IntentUtils.safeGetStringExtra(
                intent, WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME);

        // Share Target when shareActivityClassName is present.
        if (!TextUtils.isEmpty(shareActivityClassName)) {
            shareData = new ShareData();
            shareData.shareActivityClassName = shareActivityClassName;
            shareData.subject = IntentUtils.safeGetStringExtra(intent, Intent.EXTRA_SUBJECT);
            shareData.text = IntentUtils.safeGetStringExtra(intent, Intent.EXTRA_TEXT);
            shareData.files = IntentUtils.getParcelableArrayListExtra(intent, Intent.EXTRA_STREAM);
            if (shareData.files == null) {
                Uri file = IntentUtils.safeGetParcelableExtra(intent, Intent.EXTRA_STREAM);
                if (file != null) {
                    shareData.files = new ArrayList<>();
                    shareData.files.add(file);
                }
            }
        }

        String url = urlFromIntent(intent);
        int source = sourceFromIntent(intent);

        if (source == ShortcutSource.EXTERNAL_INTENT) {
            if (IntentHandler.determineExternalIntentSource(intent)
                    == IntentHandler.ExternalAppId.CHROME) {
                source = ShortcutSource.EXTERNAL_INTENT_FROM_CHROME;
            }
        }

        if (source == ShortcutSource.WEBAPK_SHARE_TARGET && shareData != null
                && shareData.files != null && shareData.files.size() > 0) {
            source = ShortcutSource.WEBAPK_SHARE_TARGET_FILE;
        }

        boolean canUseSplashFromContentProvider = IntentUtils.safeGetBooleanExtra(
                intent, WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK, false);

        return create(webApkPackageName, url, source, forceNavigation,
                canUseSplashFromContentProvider, shareData);
    }

    /**
     * Returns whether the WebAPK has a content provider which provides an image to use for the
     * splash screen.
     */
    private static boolean hasContentProviderForSplash(String webApkPackageName) {
        PackageManager packageManager = ContextUtils.getApplicationContext().getPackageManager();
        ProviderInfo providerInfo = packageManager.resolveContentProvider(
                WebApkCommonUtils.generateSplashContentProviderAuthority(webApkPackageName), 0);
        return (providerInfo != null
                && TextUtils.equals(providerInfo.packageName, webApkPackageName));
    }

    private static @WebApkDistributor int getDistributor(Bundle bundle, String packageName) {
        String distributor = IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.DISTRIBUTOR);
        if (!TextUtils.isEmpty(distributor)) {
            if (TextUtils.equals(distributor, "browser")) {
                return WebApkDistributor.BROWSER;
            }
            if (TextUtils.equals(distributor, "device_policy")) {
                return WebApkDistributor.DEVICE_POLICY;
            }
            return WebApkDistributor.OTHER;
        }
        return packageName.startsWith(WebApkConstants.WEBAPK_PACKAGE_PREFIX)
                ? WebApkDistributor.BROWSER
                : WebApkDistributor.OTHER;
    }

    /**
     * Constructs a WebApkInfo from the passed in parameters and <meta-data> in the WebAPK's Android
     * manifest.
     *
     * @param webApkPackageName The package name of the WebAPK.
     * @param url Url that the WebAPK should navigate to when launched.
     * @param source Source that the WebAPK was launched from.
     * @param forceNavigation Whether the WebAPK should navigate to {@link url} if it is already
     *                        running.
     * @param canUseSplashFromContentProvider Whether the WebAPK's content provider can be
     *                                        queried for a screenshot of the splash screen.
     * @param shareData Shared information from the share intent.
     */
    public static WebApkInfo create(String webApkPackageName, String url, int source,
            boolean forceNavigation, boolean canUseSplashFromContentProvider, ShareData shareData) {
        // Unlike non-WebAPK web apps, WebAPK ids are predictable. A malicious actor may send an
        // intent with a valid start URL and arbitrary other data. Only use the start URL, the
        // package name and the ShortcutSource from the launch intent and extract the remaining data
        // from the <meta-data> in the WebAPK's Android manifest.

        Bundle bundle = extractWebApkMetaData(webApkPackageName);
        if (bundle == null) {
            return null;
        }

        Context appContext = ContextUtils.getApplicationContext();
        Resources res = null;
        try {
            res = appContext.getPackageManager().getResourcesForApplication(webApkPackageName);
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }

        int nameId = res.getIdentifier(RESOURCE_NAME, RESOURCE_STRING_TYPE, webApkPackageName);
        int shortNameId =
                res.getIdentifier(RESOURCE_SHORT_NAME, RESOURCE_STRING_TYPE, webApkPackageName);
        String name = nameId != 0 ? res.getString(nameId)
                                  : IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.NAME);
        String shortName = shortNameId != 0
                ? res.getString(shortNameId)
                : IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.SHORT_NAME);

        String scope = IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.SCOPE);

        @WebDisplayMode
        int displayMode = displayModeFromString(
                IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.DISPLAY_MODE));
        int orientation = orientationFromString(
                IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.ORIENTATION));
        long themeColor = WebApkMetaDataUtils.getLongFromMetaData(bundle,
                WebApkMetaDataKeys.THEME_COLOR, ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
        long backgroundColor =
                WebApkMetaDataUtils.getLongFromMetaData(bundle, WebApkMetaDataKeys.BACKGROUND_COLOR,
                        ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);

        // Fetch the default background color from the WebAPK's resources. Fetching the default
        // background color from the WebAPK is important for consistency when:
        // - A new version of Chrome has changed the default background color.
        // - Chrome has not yet requested an update for the WebAPK and the WebAPK still has the old
        //   default background color in its resources.
        // New-style WebAPKs use the background color and default background color in both the
        // WebAPK and Chrome processes.
        int defaultBackgroundColorId =
                IntentUtils.safeGetInt(bundle, WebApkMetaDataKeys.DEFAULT_BACKGROUND_COLOR_ID, 0);
        int defaultBackgroundColor = (defaultBackgroundColorId == 0)
                ? SplashLayout.getDefaultBackgroundColor(appContext)
                : ApiCompatibilityUtils.getColor(res, defaultBackgroundColorId);

        int shellApkVersion =
                IntentUtils.safeGetInt(bundle, WebApkMetaDataKeys.SHELL_APK_VERSION, 0);

        String manifestUrl = IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.WEB_MANIFEST_URL);
        String manifestStartUrl = IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.START_URL);
        Map<String, String> iconUrlToMurmur2HashMap = getIconUrlAndIconMurmur2HashMap(bundle);

        @WebApkDistributor
        int distributor = getDistributor(bundle, webApkPackageName);

        int primaryIconId = IntentUtils.safeGetInt(bundle, WebApkMetaDataKeys.ICON_ID, 0);
        Bitmap primaryIcon = decodeBitmapFromDrawable(res, primaryIconId);

        boolean isPrimaryIconMaskable =
                IntentUtils.safeGetBoolean(bundle, WebApkMetaDataKeys.IS_ICON_MASKABLE, false);

        int badgeIconId = IntentUtils.safeGetInt(bundle, WebApkMetaDataKeys.BADGE_ICON_ID, 0);
        Bitmap badgeIcon = decodeBitmapFromDrawable(res, badgeIconId);

        int splashIconId = IntentUtils.safeGetInt(bundle, WebApkMetaDataKeys.SPLASH_ID, 0);
        Bitmap splashIcon = decodeBitmapFromDrawable(res, splashIconId);

        Pair<String, ShareTarget> shareTargetActivityNameAndData =
                extractFirstShareTarget(webApkPackageName);
        String shareTargetActivityName = shareTargetActivityNameAndData.first;
        ShareTarget shareTarget = shareTargetActivityNameAndData.second;

        boolean isSplashProvidedByWebApk =
                (canUseSplashFromContentProvider && Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                        && hasContentProviderForSplash(webApkPackageName));

        return create(WebApkConstants.WEBAPK_ID_PREFIX + webApkPackageName, url, scope,
                new Icon(primaryIcon), new Icon(badgeIcon), new Icon(splashIcon), name, shortName,
                displayMode, orientation, source, themeColor, backgroundColor,
                defaultBackgroundColor, isPrimaryIconMaskable, webApkPackageName, shellApkVersion,
                manifestUrl, manifestStartUrl, distributor, iconUrlToMurmur2HashMap, shareTarget,
                shareTargetActivityName, forceNavigation, isSplashProvidedByWebApk, shareData);
    }

    /**
     * Construct a {@link WebApkInfo} instance.
     * @param id                       ID for the WebAPK.
     * @param url                      URL that the WebAPK should navigate to when launched.
     * @param scope                    Scope for the WebAPK.
     * @param primaryIcon              Primary icon to show for the WebAPK.
     * @param badgeIcon                Badge icon to use for notifications.
     * @param splashIcon               Splash icon to use for the splash screen.
     * @param name                     Name of the WebAPK.
     * @param shortName                The short name of the WebAPK.
     * @param displayMode              Display mode of the WebAPK.
     * @param orientation              Orientation of the WebAPK.
     * @param source                   Source that the WebAPK was launched from.
     * @param themeColor               The theme color of the WebAPK.
     * @param backgroundColor          The background color of the WebAPK.
     * @param defaultBackgroundColor   The background color to use if the Web Manifest does not
     *                                 provide a background color.
     * @param isPrimaryIconMaskable    Is the primary icon maskable.
     * @param webApkPackageName        The package of the WebAPK.
     * @param shellApkVersion          Version of the code in //chrome/android/webapk/shell_apk.
     * @param manifestUrl              URL of the Web Manifest.
     * @param manifestStartUrl         URL that the WebAPK should navigate to when launched from
     *                                 the homescreen. Different from the {@link url} parameter if
     *                                 the WebAPK is launched from a deep link.
     * @param distributor              The source from where the WebAPK is installed.
     * @param iconUrlToMurmur2HashMap  Map of the WebAPK's icon URLs to Murmur2 hashes of the
     *                                 icon untransformed bytes.
     * @param shareTarget              shareTarget data for {@link shareTargetActivityName}
     * @param shareTargetActivityName  Name of activity or activity alias in WebAPK which handles
     *                                 share intents
     * @param forceNavigation          Whether the WebAPK should navigate to {@link url} if the
     *                                 WebAPK is already open.
     * @param isSplashProvidedByWebApk Whether the WebAPK (1) launches an internal activity to
     *                                 display the splash screen and (2) has a content provider
     *                                 which provides a screenshot of the splash screen.
     * @param shareData                Shared information from the share intent.
     */
    public static WebApkInfo create(String id, String url, String scope, Icon primaryIcon,
            Icon badgeIcon, Icon splashIcon, String name, String shortName,
            @WebDisplayMode int displayMode, int orientation, int source, long themeColor,
            long backgroundColor, int defaultBackgroundColor, boolean isPrimaryIconMaskable,
            String webApkPackageName, int shellApkVersion, String manifestUrl,
            String manifestStartUrl, @WebApkDistributor int distributor,
            Map<String, String> iconUrlToMurmur2HashMap, ShareTarget shareTarget,
            String shareTargetActivityName, boolean forceNavigation,
            boolean isSplashProvidedByWebApk, ShareData shareData) {
        if (id == null || url == null || manifestStartUrl == null || webApkPackageName == null) {
            Log.e(TAG,
                    "Incomplete data provided: " + id + ", " + url + ", " + manifestStartUrl + ", "
                            + webApkPackageName);
            return null;
        }

        // The default scope should be computed from the Web Manifest start URL. If the WebAPK was
        // launched from a deep link {@link startUrl} may be different from the Web Manifest start
        // URL.
        if (TextUtils.isEmpty(scope)) {
            scope = ShortcutHelper.getScopeFromUrl(manifestStartUrl);
        }

        return new WebApkInfo(id, url, scope, primaryIcon, badgeIcon, splashIcon, name, shortName,
                displayMode, orientation, source, themeColor, backgroundColor,
                defaultBackgroundColor, isPrimaryIconMaskable, webApkPackageName, shellApkVersion,
                manifestUrl, manifestStartUrl, distributor, iconUrlToMurmur2HashMap, shareTarget,
                shareTargetActivityName, forceNavigation, isSplashProvidedByWebApk, shareData);
    }

    protected WebApkInfo(String id, String url, String scope, Icon primaryIcon, Icon badgeIcon,
            Icon splashIcon, String name, String shortName, @WebDisplayMode int displayMode,
            int orientation, int source, long themeColor, long backgroundColor,
            int defaultBackgroundColor, boolean isPrimaryIconMaskable, String webApkPackageName,
            int shellApkVersion, String manifestUrl, String manifestStartUrl,
            @WebApkDistributor int distributor, Map<String, String> iconUrlToMurmur2HashMap,
            ShareTarget shareTarget, String shareTargetActivityName, boolean forceNavigation,
            boolean isSplashProvidedByWebApk, ShareData shareData) {
        super(id, url, scope, primaryIcon, name, shortName, displayMode, orientation, source,
                themeColor, backgroundColor, defaultBackgroundColor, false /* isIconGenerated */,
                isPrimaryIconMaskable /* isIconAdaptive */, forceNavigation);
        mBadgeIcon = badgeIcon;
        mSplashIcon = splashIcon;
        mApkPackageName = webApkPackageName;
        mShellApkVersion = shellApkVersion;
        mManifestUrl = manifestUrl;
        mManifestStartUrl = manifestStartUrl;
        mDistributor = distributor;
        mIconUrlToMurmur2HashMap = iconUrlToMurmur2HashMap;
        mIsSplashProvidedByWebApk = isSplashProvidedByWebApk;
        mShareData = shareData;
        mShareTarget = shareTarget;
        if (mShareTarget == null) {
            mShareTarget = new ShareTarget();
        }
        mShareTargetActivityName = shareTargetActivityName;
    }

    protected WebApkInfo() {}

    /**
     * Returns the badge icon in Bitmap form.
     */
    public Bitmap badgeIcon() {
        return (mBadgeIcon == null) ? null : mBadgeIcon.decoded();
    }

    /**
     * Returns the splash icon in Bitmap form.
     */
    public Bitmap splashIcon() {
        return (mSplashIcon == null) ? null : mSplashIcon.decoded();
    }

    /** Returns data about the WebAPK's share intent handlers. */
    public ShareTarget shareTarget() {
        return mShareTarget;
    }

    /**
     * Returns name of activity or activity alias in WebAPK which handles share intents.
     */
    public String shareTargetActivityName() {
        return mShareTargetActivityName;
    }

    @Override
    public boolean isForWebApk() {
        return true;
    }

    @Override
    public String webApkPackageName() {
        return mApkPackageName;
    }

    @Override
    public boolean isSplashProvidedByWebApk() {
        return mIsSplashProvidedByWebApk;
    }

    public int shellApkVersion() {
        return mShellApkVersion;
    }

    public String manifestUrl() {
        return mManifestUrl;
    }

    public String manifestStartUrl() {
        return mManifestStartUrl;
    }

    public @WebApkDistributor int distributor() {
        return mDistributor;
    }

    public Map<String, String> iconUrlToMurmur2HashMap() {
        return mIconUrlToMurmur2HashMap;
    }

    public ShareData shareData() {
        return mShareData;
    }

    @Override
    public void setWebappIntentExtras(Intent intent) {
        // For launching a {@link WebApkActivity}.
        intent.putExtra(ShortcutHelper.EXTRA_ID, id());
        intent.putExtra(ShortcutHelper.EXTRA_URL, uri().toString());
        intent.putExtra(ShortcutHelper.EXTRA_SOURCE, source());
        intent.putExtra(ShortcutHelper.EXTRA_FORCE_NAVIGATION, shouldForceNavigation());
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, webApkPackageName());
        intent.putExtra(
                WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK, isSplashProvidedByWebApk());
    }

    /**
     * Extracts meta data from a WebAPK's Android Manifest.
     * @param webApkPackageName WebAPK's package name.
     * @return Bundle with the extracted meta data.
     */
    private static Bundle extractWebApkMetaData(String webApkPackageName) {
        PackageManager packageManager = ContextUtils.getApplicationContext().getPackageManager();
        try {
            ApplicationInfo appInfo = packageManager.getApplicationInfo(
                    webApkPackageName, PackageManager.GET_META_DATA);
            return appInfo.metaData;
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }
    }

    /**
     * Decodes bitmap drawable from WebAPK's resources. This should also be used for XML aliases.
     */
    private static Bitmap decodeBitmapFromDrawable(Resources webApkResources, int resourceId) {
        if (resourceId == 0) {
            return null;
        }
        try {
            BitmapDrawable bitmapDrawable =
                    (BitmapDrawable) ApiCompatibilityUtils.getDrawable(webApkResources, resourceId);
            return bitmapDrawable != null ? bitmapDrawable.getBitmap() : null;
        } catch (Resources.NotFoundException e) {
            return null;
        }
    }

    /**
     * Extract the icon URLs and icon hashes from the WebAPK's meta data, and returns a map of these
     * {URL, hash} pairs. The icon URLs/icon hashes are stored in a single meta data tag in the
     * WebAPK's AndroidManifest.xml as following:
     * "URL1 hash1 URL2 hash2 URL3 hash3..."
     */
    private static Map<String, String> getIconUrlAndIconMurmur2HashMap(Bundle metaData) {
        Map<String, String> iconUrlAndIconMurmur2HashMap = new HashMap<String, String>();
        String iconUrlsAndIconMurmur2Hashes = metaData.getString(
                WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES);
        if (TextUtils.isEmpty(iconUrlsAndIconMurmur2Hashes)) return iconUrlAndIconMurmur2HashMap;

        // Parse the metadata tag which contains "URL1 hash1 URL2 hash2 URL3 hash3..." pairs and
        // create a hash map.
        // TODO(hanxi): crbug.com/666349. Add a test to verify that the icon URLs in WebAPKs'
        // AndroidManifest.xml don't contain space.
        String[] urlsAndHashes = iconUrlsAndIconMurmur2Hashes.split("[ ]+");
        if (urlsAndHashes.length % 2 != 0) {
            Log.e(TAG, "The icon URLs and icon murmur2 hashes don't come in pairs.");
            return iconUrlAndIconMurmur2HashMap;
        }
        for (int i = 0; i < urlsAndHashes.length; i += 2) {
            iconUrlAndIconMurmur2HashMap.put(urlsAndHashes[i], urlsAndHashes[i + 1]);
        }
        return iconUrlAndIconMurmur2HashMap;
    }

    /**
     * Returns the WebDisplayMode which matches {@link displayMode}.
     * @param displayMode One of https://www.w3.org/TR/appmanifest/#dfn-display-modes-values
     * @return The matching WebDisplayMode. {@link WebDisplayMode#Undefined} if there is no match.
     */
    private static @WebDisplayMode int displayModeFromString(String displayMode) {
        if (displayMode == null) {
            return WebDisplayMode.UNDEFINED;
        }

        if (displayMode.equals("fullscreen")) {
            return WebDisplayMode.FULLSCREEN;
        } else if (displayMode.equals("standalone")) {
            return WebDisplayMode.STANDALONE;
        } else if (displayMode.equals("minimal-ui")) {
            return WebDisplayMode.MINIMAL_UI;
        } else if (displayMode.equals("browser")) {
            return WebDisplayMode.BROWSER;
        } else {
            return WebDisplayMode.UNDEFINED;
        }
    }

    /**
     * Returns the ScreenOrientationValue which matches {@link orientation}.
     * @param orientation One of https://w3c.github.io/screen-orientation/#orientationlocktype-enum
     * @return The matching ScreenOrientationValue. {@link ScreenOrientationValues#DEFAULT} if there
     * is no match.
     */
    private static int orientationFromString(String orientation) {
        if (orientation == null) {
            return ScreenOrientationValues.DEFAULT;
        }

        if (orientation.equals("any")) {
            return ScreenOrientationValues.ANY;
        } else if (orientation.equals("natural")) {
            return ScreenOrientationValues.NATURAL;
        } else if (orientation.equals("landscape")) {
            return ScreenOrientationValues.LANDSCAPE;
        } else if (orientation.equals("landscape-primary")) {
            return ScreenOrientationValues.LANDSCAPE_PRIMARY;
        } else if (orientation.equals("landscape-secondary")) {
            return ScreenOrientationValues.LANDSCAPE_SECONDARY;
        } else if (orientation.equals("portrait")) {
            return ScreenOrientationValues.PORTRAIT;
        } else if (orientation.equals("portrait-primary")) {
            return ScreenOrientationValues.PORTRAIT_PRIMARY;
        } else if (orientation.equals("portrait-secondary")) {
            return ScreenOrientationValues.PORTRAIT_SECONDARY;
        } else {
            return ScreenOrientationValues.DEFAULT;
        }
    }

    /**
     * Returns the name of activity or activity alias in WebAPK which handles share intents, and
     * the data about the handler.
     */
    private static Pair<String, ShareTarget> extractFirstShareTarget(String webApkPackageName) {
        Intent shareIntent = new Intent();
        shareIntent.setAction(Intent.ACTION_SEND);
        shareIntent.setPackage(webApkPackageName);
        shareIntent.setType("*/*");
        List<ResolveInfo> resolveInfos =
                ContextUtils.getApplicationContext().getPackageManager().queryIntentActivities(
                        shareIntent, PackageManager.GET_META_DATA);

        for (ResolveInfo resolveInfo : resolveInfos) {
            Bundle shareTargetMetaData = resolveInfo.activityInfo.metaData;
            if (shareTargetMetaData == null) {
                continue;
            }

            String shareTargetActivityName = resolveInfo.activityInfo.name;

            String shareAction =
                    IntentUtils.safeGetString(shareTargetMetaData, WebApkMetaDataKeys.SHARE_ACTION);
            if (TextUtils.isEmpty(shareAction)) {
                return new Pair<>(null, new ShareTarget());
            }

            String encodedFileNames = IntentUtils.safeGetString(
                    shareTargetMetaData, WebApkMetaDataKeys.SHARE_PARAM_NAMES);
            String[] fileNames = WebApkShareTargetUtil.decodeJsonStringArray(encodedFileNames);

            String encodedFileAccepts = IntentUtils.safeGetString(
                    shareTargetMetaData, WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS);
            String[][] fileAccepts = WebApkShareTargetUtil.decodeJsonAccepts(encodedFileAccepts);

            String shareMethod =
                    IntentUtils.safeGetString(shareTargetMetaData, WebApkMetaDataKeys.SHARE_METHOD);
            boolean isShareMethodPost =
                    shareMethod != null && shareMethod.toUpperCase(Locale.ENGLISH).equals("POST");

            String shareEncType = IntentUtils.safeGetString(
                    shareTargetMetaData, WebApkMetaDataKeys.SHARE_ENCTYPE);
            boolean isShareEncTypeMultipart = shareEncType != null
                    && shareEncType.toLowerCase(Locale.ENGLISH).equals("multipart/form-data");

            ShareTarget target = new ShareTarget(
                    IntentUtils.safeGetString(shareTargetMetaData, WebApkMetaDataKeys.SHARE_ACTION),
                    IntentUtils.safeGetString(
                            shareTargetMetaData, WebApkMetaDataKeys.SHARE_PARAM_TITLE),
                    IntentUtils.safeGetString(
                            shareTargetMetaData, WebApkMetaDataKeys.SHARE_PARAM_TEXT),
                    IntentUtils.safeGetString(
                            shareTargetMetaData, WebApkMetaDataKeys.SHARE_PARAM_URL),
                    isShareMethodPost, isShareEncTypeMultipart, fileNames, fileAccepts);

            return new Pair<>(shareTargetActivityName, target);
        }
        return new Pair<>(null, new ShareTarget());
    }

    /** Returns the value if it is non-null. Returns an empty string otherwise. */
    private static String replaceNullWithEmpty(String value) {
        return (value == null) ? "" : value;
    }
}
