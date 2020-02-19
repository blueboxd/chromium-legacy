// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const FilesMetadataEntry = Polymer({
  is: 'files-metadata-entry',

  properties: {
    key: {
      type: String,
      reflectToAttribute: true,
    },

    // If |value| is empty, the entire entry will be hidden.
    value: {
      type: String,
      reflectToAttribute: true,
    },

    loading: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
    },

    /**
     * True if files-ng is enabled.
     * @const @type {boolean}
     * @private
     */
    filesNg_: {
      type: Boolean,
      value: util.isFilesNg(),
    }
  },

  /**
   * On element creation, set the files-ng attribute to enable files-ng
   * specific CSS styling.
   */
  created: function() {
    if (this.filesNg_) {
      this.setAttribute('files-ng', '');
    }
  },
});
