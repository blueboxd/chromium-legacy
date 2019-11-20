// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.api.host.config;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.support.annotation.IntDef;
import com.google.android.libraries.feed.common.logging.Logger;

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

  @IntDef({
    BuildType.UNKNOWN_BUILD_TYPE,
    BuildType.DEV,
    BuildType.ALPHA,
    BuildType.BETA,
    BuildType.RELEASE
  })
  public @interface BuildType {
    int UNKNOWN_BUILD_TYPE = 0;
    int DEV = 1;
    int ALPHA = 2;
    int BETA = 3;
    int RELEASE = 4;
  }

  @AppType private final int appType;
  @Architecture private final int architecture;
  @BuildType private final int buildType;
  private final String versionString;

  private ApplicationInfo(int appType, int architecture, int buildType, String versionString) {
    this.appType = appType;
    this.architecture = architecture;
    this.buildType = buildType;
    this.versionString = versionString;
  }

  @AppType
  public int getAppType() {
    return appType;
  }

  @Architecture
  public int getArchitecture() {
    return architecture;
  }

  @BuildType
  public int getBuildType() {
    return buildType;
  }

  public String getVersionString() {
    return versionString;
  }

  /** Builder class used to create {@link ApplicationInfo} objects. */
  public static final class Builder {
    private static final String TAG = "Builder";

    private final Context context;
    @AppType private int appType = AppType.UNKNOWN_APP;
    @Architecture private int architecture = Architecture.UNKNOWN_ACHITECTURE;
    @BuildType private int buildType = BuildType.UNKNOWN_BUILD_TYPE;

    private String versionString;

    public Builder(Context context) {
      this.context = context;
    }

    /** Sets the type of client application. */
    public Builder setAppType(@AppType int appType) {
      this.appType = appType;
      return this;
    }

    /** Sets the CPU architecture that the client application was built for. */
    public Builder setArchitecture(@Architecture int architecture) {
      this.architecture = architecture;
      return this;
    }

    /** Sets the release stage of the build for the client application. */
    public Builder setBuildType(@BuildType int buildType) {
      this.buildType = buildType;
      return this;
    }

    /**
     * Sets the major/minor/build/revision numbers of the application version from the given string.
     * A version string typically looks like: 'major.minor.build.revision'. If not set here, it will
     * be retrieved from the application's versionName string defined in the manifest (see
     * https://developer.android.com/studio/publish/versioning).
     */
    public Builder setVersionString(String versionString) {
      this.versionString = versionString;
      return this;
    }

    public ApplicationInfo build() {
      if (versionString == null) {
        versionString = getDefaultVersionString();
      }
      return new ApplicationInfo(appType, architecture, buildType, versionString);
    }

    private String getDefaultVersionString() {
      try {
        PackageInfo pInfo = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
        return pInfo.versionName;
      } catch (NameNotFoundException e) {
        Logger.w(TAG, e, "Cannot find package name.");
      }
      return "";
    }
  }
}
