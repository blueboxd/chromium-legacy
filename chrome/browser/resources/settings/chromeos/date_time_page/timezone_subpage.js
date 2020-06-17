// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'timezone-subpage' is the collapsible section containing
 * time zone settings.
 */
Polymer({
  is: 'timezone-subpage',

  behaviors:
      [PrefsBehavior, WebUIListenerBehavior, settings.RouteObserverBehavior],

  properties: {
    /**
     * This is <timezone-selector> parameter.
     */
    activeTimeZoneDisplayName: {
      type: String,
      notify: true,
    },
  },

  /** @private {?settings.TimeZoneBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.TimeZoneBrowserProxyImpl.getInstance();
  },

  /**
   * settings.RouteObserverBehavior
   * Called when the timezone subpage is hit. Child accounts need parental
   * approval to modify their timezone, this method starts this process on the
   * C++ side, and timezone setting will be disable. Once it is complete the
   * 'access-code-validation-complete' event is triggered which invokes
   * enableTimeZoneSetting_.
   * @param {!settings.Route} newRoute
   * @protected
   */
  currentRouteChanged(newRoute) {
    if (this.shouldAskForParentAccessCode_(newRoute)) {
      this.disableTimeZoneSetting_();
      this.addWebUIListener(
          'access-code-validation-complete',
          this.enableTimeZoneSetting_.bind(this));
      this.browserProxy_.showParentAccessForTimeZone();
    }
  },

  /**
   * Returns value list for timeZoneResolveMethodDropdown menu.
   * @private
   */
  getTimeZoneResolveMethodsList_() {
    const result = [];
    const pref =
        this.getPref('generated.resolve_timezone_by_geolocation_method_short');
    // Make sure current value is in the list, even if it is not
    // user-selectable.
    if (pref.value == settings.TimeZoneAutoDetectMethod.DISABLED) {
      // If disabled by policy, show the 'Automatic timezone disabled' label.
      // Otherwise, just show the default string, since the control will be
      // disabled as well.
      const label = pref.controlledBy ?
          loadTimeData.getString('setTimeZoneAutomaticallyDisabled') :
          loadTimeData.getString('setTimeZoneAutomaticallyIpOnlyDefault');
      result.push(
          {value: settings.TimeZoneAutoDetectMethod.DISABLED, name: label});
    }
    result.push({
      value: settings.TimeZoneAutoDetectMethod.IP_ONLY,
      name: loadTimeData.getString('setTimeZoneAutomaticallyIpOnlyDefault')
    });

    if (pref.value ==
        settings.TimeZoneAutoDetectMethod.SEND_WIFI_ACCESS_POINTS) {
      result.push({
        value: settings.TimeZoneAutoDetectMethod.SEND_WIFI_ACCESS_POINTS,
        name: loadTimeData.getString(
            'setTimeZoneAutomaticallyWithWiFiAccessPointsData')
      });
    }
    result.push({
      value: settings.TimeZoneAutoDetectMethod.SEND_ALL_LOCATION_INFO,
      name:
          loadTimeData.getString('setTimeZoneAutomaticallyWithAllLocationInfo')
    });
    return result;
  },

  /**
   * @param {!settings.Route} route
   * @private
   */
  shouldAskForParentAccessCode_(route) {
    return route === settings.routes.DATETIME_TIMEZONE_SUBPAGE &&
        loadTimeData.getBoolean('isChild');
  },

  /**
   * Enables all dropdowns and radio buttons.
   * @private
   */
  enableTimeZoneSetting_() {
    const radios = this.root.querySelectorAll('controlled-radio-button');
    for (const radio of radios) {
      radio.disabled = false;
    }
    this.$.timezoneSelector.shouldDisableTimeZoneGeoSelector = false;
    const pref =
        this.getPref('generated.resolve_timezone_by_geolocation_method_short');
    if (pref.value !== settings.TimeZoneAutoDetectMethod.DISABLED) {
      this.$.timeZoneResolveMethodDropdown.disabled = false;
    }
  },

  /**
   * Disables all dropdowns and radio buttons.
   * @private
   */
  disableTimeZoneSetting_() {
    this.$.timeZoneResolveMethodDropdown.disabled = true;
    this.$.timezoneSelector.shouldDisableTimeZoneGeoSelector = true;
    const radios = this.root.querySelectorAll('controlled-radio-button');
    for (const radio of radios) {
      radio.disabled = true;
    }
  },

});
