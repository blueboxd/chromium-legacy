// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('welcome');

/**
 * @typedef {{
 *   id: number,
 *   name: string,
 *   icon: string,
 *   url: string,
 *   bookmarkId: ?string,
 *   selected: boolean,
 * }}
 */
welcome.AppItem;

/**
 * @typedef {{
 *   item: !welcome.AppItem,
 *   set: function(string, boolean):void
 * }}
 */
welcome.AppItemModel;

const KEYBOARD_FOCUSED = 'keyboard-focused';

Polymer({
  is: 'nux-google-apps',

  behaviors: [welcome.NavigationBehavior, I18nBehavior],

  properties: {
    /** @type {welcome.stepIndicatorModel} */
    indicatorModel: Object,

    /**
     * @type {!Array<!welcome.AppItem>}
     * @private
     */
    appList_: Array,

    hasAppsSelected_: {
      type: Boolean,
      notify: true,
      value: true,
    },
  },

  /** @private {welcome.GoogleAppProxy} */
  appProxy_: null,

  /** @private {?welcome.ModuleMetricsManager} */
  metricsManager_: null,

  /** @private */
  finalized_: false,

  /** @private {welcome.BookmarkProxy} */
  bookmarkProxy_: null,

  /** @private {welcome.BookmarkBarManager} */
  bookmarkBarManager_: null,

  /** @private {boolean} */
  wasBookmarkBarShownOnInit_: false,

  /** @override */
  ready: function() {
    this.appProxy_ = welcome.GoogleAppProxyImpl.getInstance();
    this.metricsManager_ = new welcome.ModuleMetricsManager(
        welcome.GoogleAppsMetricsProxyImpl.getInstance());
    this.bookmarkProxy_ = welcome.BookmarkProxyImpl.getInstance();
    this.bookmarkBarManager_ = welcome.BookmarkBarManager.getInstance();
  },

  /** @override */
  attached: function() {
    Polymer.RenderStatus.afterNextRender(this, () => {
      Polymer.IronA11yAnnouncer.requestAvailability();
    });
  },

  onRouteEnter: function() {
    this.finalized_ = false;
    this.metricsManager_.recordPageInitialized();
    this.populateAllBookmarks_();
  },

  onRouteExit: function() {
    if (this.finalized_) {
      return;
    }
    this.cleanUp_();
    this.metricsManager_.recordBrowserBackOrForward();
  },

  onRouteUnload: function() {
    if (this.finalized_) {
      return;
    }
    this.cleanUp_();
    this.metricsManager_.recordNavigatedAway();
  },

  /**
   * @param {EventTarget} element
   * @param {number} direction
   * @private
   */
  changeFocus_: function(element, direction) {
    if (isRTL()) {
      direction *= -1;  // Reverse direction if RTL.
    }

    const buttons = this.root.querySelectorAll('button');
    const targetIndex = Array.prototype.indexOf.call(buttons, element);

    const oldFocus = buttons[targetIndex];
    if (!oldFocus) {
      return;
    }

    const newFocus = buttons[targetIndex + direction];

    // New target and we're changing direction.
    if (newFocus && direction) {
      newFocus.classList.add(KEYBOARD_FOCUSED);
      oldFocus.classList.remove(KEYBOARD_FOCUSED);
      newFocus.focus();
    } else {
      oldFocus.classList.add(KEYBOARD_FOCUSED);
    }
  },

  /**
   * Called when bookmarks should be removed for all selected apps.
   * @private
   */
  cleanUp_: function() {
    this.finalized_ = true;

    if (!this.appList_) {
      return;
    }  // No apps to remove.

    let removedBookmarks = false;
    this.appList_.forEach(app => {
      if (app.selected && app.bookmarkId) {
        // Don't call |updateBookmark_| b/c we want to save the selection in the
        // event of a browser back/forward.
        this.bookmarkProxy_.removeBookmark(app.bookmarkId);
        app.bookmarkId = null;
        removedBookmarks = true;
      }
    });
    // Only update and announce if we removed bookmarks.
    if (removedBookmarks) {
      this.bookmarkBarManager_.setShown(this.wasBookmarkBarShownOnInit_);
      this.fire('iron-announce', {text: this.i18n('bookmarksRemoved')});
    }
  },

  /**
   * Handle toggling the apps selected.
   * @param {!{model: !welcome.AppItemModel}} e
   * @private
   */
  onAppClick_: function(e) {
    const item = e.model.item;

    e.model.set('item.selected', !item.selected);

    this.updateBookmark_(item);
    this.updateHasAppsSelected_();

    this.metricsManager_.recordClickedOption();

    // Announcements should NOT be in |updateBookmark_| because there should be
    // a different utterance when all app bookmarks are added/removed.
    const i18nKey = item.selected ? 'bookmarkAdded' : 'bookmarkRemoved';
    this.fire('iron-announce', {text: this.i18n(i18nKey)});
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAppKeyUp_: function(e) {
    if (e.key == 'ArrowRight') {
      this.changeFocus_(e.currentTarget, 1);
    } else if (e.key == 'ArrowLeft') {
      this.changeFocus_(e.currentTarget, -1);
    } else {
      e.currentTarget.classList.add(KEYBOARD_FOCUSED);
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAppPointerDown_: function(e) {
    e.currentTarget.classList.remove(KEYBOARD_FOCUSED);
  },

  /** @private */
  onGetStartedClicked_: function() {
    this.finalized_ = true;
    this.appList_.forEach(app => {
      if (app.selected) {
        this.appProxy_.recordProviderSelected(app.id);
      }
    });
    this.metricsManager_.recordGetStarted();
    welcome.navigateToNextStep();
  },

  /** @private */
  onNoThanksClicked_: function() {
    this.cleanUp_();
    this.metricsManager_.recordNoThanks();
    welcome.navigateToNextStep();
  },

  /**
   * Called when bookmarks should be created for all selected apps.
   * @private
   */
  populateAllBookmarks_: function() {
    this.wasBookmarkBarShownOnInit_ = this.bookmarkBarManager_.getShown();

    if (this.appList_) {
      this.appList_.forEach(app => this.updateBookmark_(app));
    } else {
      this.appProxy_.getAppList().then(list => {
        this.appList_ = /** @type(!Array<!welcome.AppItem>) */ (list);
        this.appList_.forEach((app, index) => {
          // Default select first few items.
          app.selected = index < 3;
          this.updateBookmark_(app);
        });
        this.updateHasAppsSelected_();
        this.fire('iron-announce', {text: this.i18n('bookmarksAdded')});
      });
    }
  },

  /**
   * @param {!welcome.AppItem} item
   * @private
   */
  updateBookmark_: function(item) {
    if (item.selected && !item.bookmarkId) {
      this.bookmarkBarManager_.setShown(true);
      this.bookmarkProxy_.addBookmark(
          {
            title: item.name,
            url: item.url,
            parentId: '1',
          },
          result => {
            item.bookmarkId = result.id;
          });
      // Cache bookmark icon.
      this.appProxy_.cacheBookmarkIcon(item.id);
    } else if (!item.selected && item.bookmarkId) {
      this.bookmarkProxy_.removeBookmark(item.bookmarkId);
      item.bookmarkId = null;
    }
  },

  /**
   * Updates the value of hasAppsSelected_.
   * @private
   */
  updateHasAppsSelected_: function() {
    this.hasAppsSelected_ =
        this.appList_ && this.appList_.some(a => a.selected);
    if (!this.hasAppsSelected_) {
      this.bookmarkBarManager_.setShown(this.wasBookmarkBarShownOnInit_);
    }
  },

  /**
   * Converts a boolean to a string because aria-pressed needs a string value.
   * @param {boolean} value
   * @return {string}
   * @private
   */
  getAriaPressed_: function(value) {
    return value ? 'true' : 'false';
  }
});
