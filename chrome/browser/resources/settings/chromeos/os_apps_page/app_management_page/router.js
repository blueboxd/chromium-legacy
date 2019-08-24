// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-router',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /** @private {string} */
    path_: String,

    /** @private {Object} */
    queryParams_: Object,

    /** @private {string} */
    query_: {
      type: String,
      observer: 'onQueryChanged_',
    },

    /** @private {string} */
    urlQuery_: {
      type: String,
      observer: 'onUrlQueryChanged_',
    },

    /** @private */
    searchTerm_: {
      type: String,
      value: '',
    },

    /** @private {PageType} */
    currentPageType_: {
      type: Number,
    },

    /** @private {?string} */
    selectedAppId_: {
      type: String,
    },
  },

  urlParsed_: false,

  observers: [
    'onUrlChanged_(path_, queryParams_)',
    'onStateChanged_(currentPageType_, selectedAppId_, searchTerm_)',
  ],

  attached: function() {
    this.watch('currentPageType_', (state) => {
      return state.currentPage.pageType;
    });
    this.watch('selectedAppId_', (state) => {
      return state.currentPage.selectedAppId;
    });
    this.watch('searchTerm_', (state) => {
      return state.search.term;
    });
    this.updateFromStore();
  },

  /**
   * @param {?string} current Current value of the query.
   * @param {?string} previous Previous value of the query.
   * @private
   */
  onQueryChanged_: function(current, previous) {
    if (previous !== undefined) {
      this.urlQuery_ = this.query_;
    }
  },

  /** @private */
  onUrlQueryChanged_: function() {
    this.query_ = this.urlQuery_;
  },

  /** @private */
  onStateChanged_: function() {
    if (!this.urlParsed_) {
      return;
    }
    this.debounce('publishUrl', this.publishUrl_);
  },

  /** @private */
  publishUrl_: function() {
    // Disable pushing urls into the history stack, so that we only push one
    // state.
    this.$['iron-location'].dwellTime = Infinity;
    this.publishQueryParams_();
    // Re-enable pushing urls into the history stack.
    this.$['iron-location'].dwellTime = 0;
    this.publishPath_();
  },

  /** @private */
  publishQueryParams_: function() {
    const newQueryParams = Object.assign({}, this.queryParams_);

    newQueryParams.q = this.searchTerm_ || undefined;
    newQueryParams.id = this.selectedAppId_ || undefined;

    // Can't update |this.queryParams_| every time since assigning a new object
    // to it triggers a state change which causes the URL to change, which
    // recurses into a loop. JSON.stringify is used here to compare objects as
    // it is always going to be a key value (string) pair and will serialize
    // correctly.
    if (JSON.stringify(newQueryParams) !== JSON.stringify(this.queryParams_)) {
      this.queryParams_ = newQueryParams;
    }
  },

  /** @private */
  publishPath_: function() {
    let path = '';
    if (this.currentPageType_ === PageType.DETAIL) {
      path = 'detail';
    } else if (this.currentPageType_ === PageType.NOTIFICATIONS) {
      path = 'notifications';
    }
    this.path_ = '/' + path;
  },

  /** @private */
  onUrlChanged_: function() {
    this.debounce('parseUrl', this.parseUrl_);
  },

  /** @private */
  parseUrl_: function() {
    const newId = this.queryParams_.id;
    const searchTerm = this.queryParams_.q;

    const pageFromUrl = this.path_.substr(1).split('/')[0];
    let newPage = PageType.MAIN;
    if (pageFromUrl === 'detail') {
      newPage = PageType.DETAIL;
    } else if (pageFromUrl === 'notifications') {
      newPage = PageType.NOTIFICATIONS;
    } else {
      newPage = PageType.MAIN;
    }

    if (newPage === PageType.DETAIL) {
      this.dispatch(app_management.actions.changePage(PageType.DETAIL, newId));
    } else {
      this.dispatch(app_management.actions.changePage(newPage));
    }

    if (searchTerm) {
      this.dispatch(app_management.actions.setSearchTerm(searchTerm));
    }
    this.urlParsed_ = true;
  },
});
