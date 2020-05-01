// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_USER_AGENT_H_
#define CONTENT_PUBLIC_COMMON_USER_AGENT_H_

#include <string>

#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/user_agent/user_agent_metadata.mojom.h"

namespace content {

namespace frozen_user_agent_strings {

const char kDesktop[] =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, "
    "like Gecko) Chrome/%s.0.0.0 Safari/537.36";
const char kAndroid[] =
    "Mozilla/5.0 (Linux; Android 9; Unspecified Device) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/%s.0.0.0 "
    "Safari/537.36";
const char kAndroidMobile[] =
    "Mozilla/5.0 (Linux; Android 9; Unspecified Device) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/%s.0.0.0 Mobile "
    "Safari/537.36";

}  // namespace frozen_user_agent_strings

enum class IncludeAndroidBuildNumber { Include, Exclude };
enum class IncludeAndroidModel { Include, Exclude };

// Returns the WebKit version, in the form "major.minor (branch@revision)".
CONTENT_EXPORT std::string GetWebKitVersion();

CONTENT_EXPORT std::string GetWebKitRevision();

// Builds a string that describes the CPU type when available (or blank
// otherwise).
CONTENT_EXPORT std::string BuildCpuInfo();

// Builds a User-agent compatible string that describes the OS and CPU type.
// On Android, the string will only include the build number and model if
// relevant enums indicate they should be included.
CONTENT_EXPORT std::string BuildOSCpuInfo(
    IncludeAndroidBuildNumber include_android_build_number,
    IncludeAndroidModel include_android_model);

// Returns the OS version.
// On Android, the string will only include the build number and model if
// relevant enums indicate they should be included.
CONTENT_EXPORT std::string GetOSVersion(
    IncludeAndroidBuildNumber include_android_build_number,
    IncludeAndroidModel include_android_model);

// Returns the frozen User-agent string for
// https://github.com/WICG/ua-client-hints.
CONTENT_EXPORT std::string GetFrozenUserAgent(bool mobile,
                                              std::string major_version);

// Helper function to generate a full user agent string from a short
// product name.
CONTENT_EXPORT std::string BuildUserAgentFromProduct(
    const std::string& product);

// Returns the model information. Returns a blank string if not on Android or
// if on a codenamed (i.e. not a release) build of an Android.
CONTENT_EXPORT std::string BuildModelInfo();

#if defined(OS_ANDROID)
// Helper function to generate a full user agent string given a short
// product name and some extra text to be added to the OS info.
// This is currently only used for Android Web View.
CONTENT_EXPORT std::string BuildUserAgentFromProductAndExtraOSInfo(
    const std::string& product,
    const std::string& extra_os_info,
    IncludeAndroidBuildNumber include_android_build_number);

// Helper function to generate just the OS info.
CONTENT_EXPORT std::string GetAndroidOSInfo(
    IncludeAndroidBuildNumber include_android_build_number,
    IncludeAndroidModel include_android_model);
#endif

// Builds a full user agent string given a string describing the OS and a
// product name.
CONTENT_EXPORT std::string BuildUserAgentFromOSAndProduct(
    const std::string& os_info,
    const std::string& product);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_USER_AGENT_H_
