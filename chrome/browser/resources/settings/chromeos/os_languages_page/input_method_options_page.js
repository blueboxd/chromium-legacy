// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-input-method-options-page' is the settings sub-page
 * to allow users to change options for each input method.
 */

Polymer({
  is: 'settings-input-method-options-page',

  behaviors: [
    settings.RouteObserverBehavior,
  ],

  properties: {
    /** @type {!LanguageHelper} */
    languageHelper: Object,

    /**
     * Input method ID.
     * @private
     */
    id_: String,
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    if (route != settings.routes.OS_LANGUAGES_INPUT_METHOD_OPTIONS) {
      this.id_ = '';
      this.parentNode.pageTitle = '';
      return;
    }

    const queryParams = settings.Router.getInstance().getQueryParameters();
    this.id_ = queryParams.get('id') || '';
    this.parentNode.pageTitle =
        this.languageHelper.getInputMethodDisplayName(this.id_);
  },
});
