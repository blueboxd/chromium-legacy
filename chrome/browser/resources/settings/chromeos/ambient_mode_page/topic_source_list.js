// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of
 * AmbientModeTopicSource.
 */

/**
 * Polymer class definition for 'topic-source-list'.
 */
Polymer({
  is: 'topic-source-list',

  properties: {
    /**
     * Contains topic sources.
     * @private {!Array<!AmbientModeTopicSource>}
     */
    topicSources: {
      type: Array,
      value: [],
    },

    /**
     * Reflects the iron-list selectedItem property.
     * @type {AmbientModeTopicSource}
     */
    selectedItem: Object,

    /**
     * The last iron-list selectedItem.
     * @type {AmbientModeTopicSource}
     */
    lastSelectedItem_: Object,
  },

  /** @private */
  onSelectedItemChanged_() {
    // <iron-list> causes |this.$.topicSourceList.selectedItem| to be null if
    // tapped a second time.
    if (this.$.topicSourceList.selectedItem === null) {
      // In the case that the user deselects an item, reselect the item manually
      // by altering the list.
      this.selectedItem_ = this.lastSelectedItem_;
      return;
    }

    if (this.lastSelectedItem_ !== this.$.topicSourceList.selectedItem) {
      this.lastSelectedItem_ = this.$.topicSourceList.selectedItem;
      this.fire('selected-topic-source-changed', this.lastSelectedItem_);
    }
  },

  /**
   * @param {!AmbientModeTopicSource} item
   * @private
   */
  isItemSelected_(item) {
    return this.selectedItem === item;
  },
});
