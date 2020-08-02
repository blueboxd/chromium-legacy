// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying options to select a
 * AmbientModeTopicSource in a list.
 */

Polymer({
  is: 'topic-source-item',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Whether this item is selected. This property is related to
     * cr_radio_button_style and used to style the disc appearance.
     */
    checked: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** @type {!AmbientModeTopicSource} */
    item: Object,

    buttonLabel: {
      type: String,
      computed: 'getButtonLabel_(item)',
    },

    /** Aria label for the row. */
    ariaLabel: {
      type: String,
      computed: 'computeAriaLabel_(item, checked)',
      reflectToAttribute: true,
    },
  },

  /** @override */
  attached() {
    this.listen(this, 'keydown', 'onKeydown_');
  },

  /** @override */
  detached() {
    this.unlisten(this, 'keydown', 'onKeydown_');
  },

  /**
   * @return {string}
   * @private
   */
  getItemName_() {
    if (this.item === AmbientModeTopicSource.GOOGLE_PHOTOS) {
      return this.i18n('ambientModeTopicSourceGooglePhotos');
    } else if (this.item === AmbientModeTopicSource.ART_GALLERY) {
      return this.i18n('ambientModeTopicSourceArtGallery');
    } else {
      return '';
    }
  },

  /**
   * @return {string}
   * @private
   */
  getItemDescription_() {
    if (this.item === AmbientModeTopicSource.GOOGLE_PHOTOS) {
      return this.i18n('ambientModeTopicSourceGooglePhotosDescription');
    } else if (this.item === AmbientModeTopicSource.ART_GALLERY) {
      return this.i18n('ambientModeTopicSourceArtGalleryDescription');
    } else {
      return '';
    }
  },

  /**
   * The aria label for the subpage button.
   * @return {string}
   * @private
   */
  getButtonLabel_() {
    return this.i18n('ambientModeTopicSourceSubpage', this.getItemName_());
  },

  /**
   * @return {string} Aria label string for ChromeVox to verbalize.
   * @private
   */
  computeAriaLabel_() {
    const rowLabel = this.getItemName_() + ' ' + this.getItemDescription_();
    if (this.checked) {
      return this.i18n(
          'ambientModeTopicSourceSelectedRow', rowLabel, this.getItemName_());
    }
    return this.i18n('ambientModeTopicSourceUnselectedRow', rowLabel);
  },

  /**
   * @param {!KeyboardEvent} event
   * @private
   */
  onKeydown_(event) {
    // The only key event handled by this element is pressing Enter.
    // (1) Pressing the subpage arrow leads to the subpage.
    // (2) Pressing anywhere on an already-selected item leads to the subpage.
    if (event.key !== 'Enter') {
      return;
    }

    // If the item is not checked, it will be handled by
    // onSelectedItemChanged_() in topic_source_list.js.
    if (!this.checked &&
        this.$$('#subpage-button') !== this.shadowRoot.activeElement) {
      return;
    }

    this.fireShowAlbums_();
    event.preventDefault();
    event.stopPropagation();
  },

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onItemClick_(event) {
    // Clicking anywhere on an already-selected item leads to the subpage.
    if (this.checked) {
      this.fireShowAlbums_();
      event.stopPropagation();
    }
  },

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onSubpageArrowClick_(event) {
    this.fireShowAlbums_();
    event.stopPropagation();
  },

  /**
   * Fires a 'show-albums' event with |this.item| as the details.
   * @private
   */
  fireShowAlbums_() {
    this.fire('show-albums', this.item);
  },
});
