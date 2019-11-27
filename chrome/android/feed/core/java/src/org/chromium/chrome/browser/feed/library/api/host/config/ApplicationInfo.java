// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.config;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.support.annotation.IntDef;

import org.chromium.chrome.browser.feed.library.common.logging.Logger;

/** API to allow the Feed to get information about the host application. */
// TODO: This can't be final because we mock it
public class ApplicationInfo {
    @IntDef({AppType.UNKNOWN_APP, AppType.SEARCH_APP, AppType.CHROME, AppType.TEST_APP})
    public @interface AppType {
        int UNKNOWN_APP = 0;
        int SEARCH_APP = 1;
        int CHROME = 2;
        int TEST_APP = 3;
    }

    @IntDef({
            Architecture.UNKNOWN_ACHITECTURE,
            Architecture.ARM,
            Architecture.ARM64,
            Architecture.MIPS,
            Architecture.MIPS64,
            Architecture.X86,
            Architecture.X86_64,
    })
    public @interface Architecture {
        int UNKNOWN_ACHITECTURE = 0;
        int ARM = 1;
        int ARM64 = 2;
        int MIPS = 3;
        int MIPS64 = 4;
        int X86 = 5;
        int X86_64 = 6;
    }

    @IntDef({BuildType.UNKNOWN_BUILD_TYPE, BuildType.DEV, BuildType.ALPHA, BuildType.BETA,
            BuildType.RELEASE})
    public @interface BuildType {
        int UNKNOWN_BUILD_TYPE = 0;
        int DEV = 1;
        int ALPHA = 2;
        int BETA = 3;
        int RELEASE = 4;
    }

    @AppType
    private final int mAppType;
    @Architecture
    private final int mArchitecture;
    @BuildType
    private final int mBuildType;
    private final String mVersionString;

    private ApplicationInfo(int appType, int architecture, int buildType, String versionString) {
        this.mAppType = appType;
        this.mArchitecture = architecture;
        this.mBuildType = buildType;
        this.mVersionString = versionString;
    }

    @AppType
    public int getAppType() {
        return mAppType;
    }

    @Architecture
    public int getArchitecture() {
        return mArchitecture;
    }

    @BuildType
    public int getBuildType() {
        return mBuildType;
    }

    public String getVersionString() {
        return mVersionString;
    }

    /** Builder class used to create {@link ApplicationInfo} objects. */
    public static final class Builder {
        private static final String TAG = "Builder";

        private final Context mContext;
        @AppType
        private int mAppType = AppType.UNKNOWN_APP;
        @Architecture
        private int mAchitecture = Architecture.UNKNOWN_ACHITECTURE;
        @BuildType
        private int mBuildType = BuildType.UNKNOWN_BUILD_TYPE;

        private String mVersionString;

        public Builder(Context context) {
            this.mContext = context;
        }

        /** Sets the type of client application. */
        public Builder setAppType(@AppType int appType) {
            this.mAppType = appType;
            return this;
        }

        /** Sets the CPU architecture that the client application was built for. */
        public Builder setArchitecture(@Architecture int architecture) {
            this.mAchitecture = architecture;
            return this;
        }

        /** Sets the release stage of the build for the client application. */
        public Builder setBuildType(@BuildType int buildType) {
            this.mBuildType = buildType;
            return this;
        }

        /**
         * Sets the major/minor/build/revision numbers of the application version from the given
         * string. A version string typically looks like: 'major.minor.build.revision'. If not set
         * here, it will be retrieved from the application's versionName string defined in the
         * manifest (see https://developer.android.com/studio/publish/versioning).
         */
        public Builder setVersionString(String versionString) {
            this.mVersionString = versionString;
            return this;
        }

        public ApplicationInfo build() {
            if (mVersionString == null) {
                mVersionString = getDefaultVersionString();
            }
            return new ApplicationInfo(mAppType, mAchitecture, mBuildType, mVersionString);
        }

        private String getDefaultVersionString() {
            try {
                PackageInfo pInfo =
                        mContext.getPackageManager().getPackageInfo(mContext.getPackageName(), 0);
                return pInfo.versionName;
            } catch (NameNotFoundException e) {
                Logger.w(TAG, e, "Cannot find package name.");
            }
            return "";
        }
    }
}
