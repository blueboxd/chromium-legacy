// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /**
   * Describes the effective policy restriction on time zone automatic
   * detection.
   * @enum {number}
   */
  const TimeZoneAutoDetectPolicyRestriction = {
    NONE: 0,
    FORCED_ON: 1,
    FORCED_OFF: 2,
  };

  /**
   * Describes values of
   * prefs.generated.resolve_timezone_by_geolocation_method_short. Must be kept
   * in sync with TimeZoneResolverManager::TimeZoneResolveMethod enum.
   * @enum {number}
   */
  const TimeZoneAutoDetectMethod = {
    DISABLED: 0,
    IP_ONLY: 1,
    SEND_WIFI_ACCESS_POINTS: 2,
    SEND_ALL_LOCATION_INFO: 3
  };

  // #cr_define_end
  return {TimeZoneAutoDetectPolicyRestriction, TimeZoneAutoDetectMethod};
});
