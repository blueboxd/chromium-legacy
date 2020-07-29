// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file provides a way to update the OneGoogleBar by using
 *  the underlying API exposed by the OneGoogleBar.
 */

/**
 * @param {string} apiName
 * @param {string} fnName
 * @param {...*} args
 * @return {!Promise}
 */
const callApi = async (apiName, fnName, ...args) => {
  const {gbar} = /** @type {!{gbar}} */ (window);
  if (!gbar) {
    return;
  }
  const api = await gbar.a[apiName]();
  return api[fnName].apply(api, args);
};

/**
 * @type {!{
 *   bar: !{
 *     setForegroundStyle: function(number): !Promise,
 *     setBackgroundColor: function(string): !Promise,
 *     setDarkMode: function(boolean): !Promise,
 *   },
 * }}
 */
const api = [{
              name: 'bar',
              apiName: 'bf',
              fns: [
                ['setForegroundStyle', 'pc'],
                ['setBackgroundColor', 'pd'],
                ['setDarkMode', 'pp'],
              ],
            }].reduce((topLevelApi, def) => {
  topLevelApi[def.name] = def.fns.reduce((apiPart, [name, fnName]) => {
    apiPart[name] = callApi.bind(null, def.apiName, fnName);
    return apiPart;
  }, {});
  return topLevelApi;
}, {});

/** @return {!Promise} */
const updateDarkMode = async () => {
  await api.bar.setDarkMode(
      window.matchMedia('(prefers-color-scheme: dark)').matches);
  // |setDarkMode(toggle)| updates the background color and foreground style.
  // The background color should always be 'transparent'.
  api.bar.setBackgroundColor('transparent');
  // The foreground style is set based on NTP theme and not dark mode.
  api.bar.setForegroundStyle(foregroundLight ? 1 : 0);
};

/** @type {boolean} */
let foregroundLight = false;

export const oneGoogleBarApi = {
  /**
   * Returns the last argument value passed into |setForegroundLight(enabled)|.
   * @return {boolean}
   */
  isForegroundLight: () => foregroundLight,

  /**
   * Updates the foreground on the OneGoogleBar to provide contrast against the
   * background.
   * @param {boolean} enabled
   */
  setForegroundLight: enabled => {
    foregroundLight = enabled;
    api.bar.setForegroundStyle(foregroundLight ? 1 : 0);
  },

  /**
   * Updates the OneGoogleBar dark mode when called as well as any time dark
   * mode is updated.
   * @type {!function()}
   */
  trackDarkModeChanges: () => {
    window.matchMedia('(prefers-color-scheme: dark)').addListener(() => {
      updateDarkMode();
    });
    updateDarkMode();
  },
};
