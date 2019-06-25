// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-main-view',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * @private {AppMap}
     */
    apps_: {
      type: Object,
      observer: 'onAppsChanged_',
    },

    /**
     * List of apps displayed before expanding the app list.
     * @private {Array<App>}
     */
    appsList: {
      type: Array,
      value: () => [],
    },

    /**
     * A set containing the ids of all the apps with notifications enabled.
     * @private {!Set<string>}
     */
    notificationAppIds_: {
      type: Object,
      observer: 'getNotificationSublabel_',
    },
  },

  attached: function() {
    this.watch('apps_', state => state.apps);
    this.watch('notificationAppIds_', state => state.notifications.allowedIds);
    this.updateFromStore();
    this.onAppsChanged_();
  },

  /**
   * @private
   */
  onAppsChanged_: function() {
    this.appsList = Object.values(this.apps_)
                        .sort(
                            (a, b) => app_management.util.alphabeticalSort(
                                assert(a.title), assert(b.title)));
  },

  /** @private */
  onClickNotificationSublabel_: function() {
    this.dispatch(app_management.actions.changePage(PageType.NOTIFICATIONS));
  },

  /**
   * Show a string with apps' |title|(s) previewed into a label, with each
   * title ellipsised if too long.
   * @private
   */
  getNotificationSublabelPieces_: async function() {
    const notificationApps =
        Array.from(this.notificationAppIds_, id => this.getState().apps[id]);

    const /** @type {string} */ label = await cr.sendWithPromise(
        'getPluralString', 'appListPreview', notificationApps.length);

    const substitutions = [];
    for (let i = 0;
         i < APP_LIST_PREVIEW_APP_TITLES && i < notificationApps.length; i++) {
      substitutions.push(notificationApps[i].title);
    }

    // Add X more apps if the length is more than APP_LIST_PREVIEW_APP_TITLES.
    if (notificationApps.length >= APP_LIST_PREVIEW_APP_TITLES + 1) {
      substitutions.push(notificationApps.length - APP_LIST_PREVIEW_APP_TITLES);
    }
    // Only APP_LIST_PREVIEW_APP_TITLES of apps' titles get ellipsised
    // if too long. the element after that is "X other apps"
    const placeholder = APP_LIST_PREVIEW_APP_TITLES + 1;
    const pieces =
        loadTimeData.getSubstitutedStringPieces(label, ...substitutions)
            .map(function(p) {
              // Make the titles of app collapsible but make the number in the
              // "X other app(s)" part non-collapsible.
              p.collapsible = !!p.arg && p.arg !== '$' + placeholder;
              return p;
            });
    return pieces;
  },

  /**
   * Create <span> for each app title previewed,
   * making certain text fragments collapsible.
   */
  getNotificationSublabel_: async function() {
    const pieces = await this.getNotificationSublabelPieces_();
    // Create <span> for each app title previewed,
    // making certain text fragments collapsible.
    const textContainer = this.$['notifications-sublabel'];
    textContainer.textContent = '';
    for (const p of pieces) {
      if (!p.value || p.value.length === 0) {
        return;
      }

      const span = document.createElement('span');
      span.textContent = p.value;
      if (p.collapsible) {
        span.classList.add('collapsible');
      }

      textContainer.appendChild(span);
    }
  },
});
