// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controller of metadata box.
 * This should be initialized with |init| method.
 *
 * @param{!MetadataModel} metadataModel
 * @param{!QuickViewModel} quickViewModel
 * @param{!FileMetadataFormatter} fileMetadataFormatter
 *
 * @constructor
 */
function MetadataBoxController(
    metadataModel, quickViewModel, fileMetadataFormatter) {
  /**
   * @type {!MetadataModel}
   * @private
   */
  this.metadataModel_ = metadataModel;

  /**
   * @type {!QuickViewModel}
   * @private
   */
  this.quickViewModel_ = quickViewModel;

  /**
   * @type {FilesMetadataBox} metadataBox
   * @private
   */
  this.metadataBox_ = null;

  /**
   * @type {FilesQuickView} quickView
   * @private
   */
  this.quickView_ = null;

  /**
   * @type {!FileMetadataFormatter}
   * @private
   */
  this.fileMetadataFormatter_ = fileMetadataFormatter;

  /**
   * @type {Entry}
   * @private
   */
  this.previousEntry_ = null;

  /**
   * @type {boolean}
   * @private
   */
  this.isDirectorySizeLoading_ = false;

  /**
   * @type {?function(!DirectoryEntry)}
   * @private
   */
  this.onDirectorySizeLoaded_ = null;
}

/**
 * Initialize the controller with quick view which will be lazily loaded.
 *
 * TODO(oka): store quickViewModel_.metadataBoxActive state to persistent
 * storage using FilesApp window.appState?
 *
 * @param{!FilesQuickView} quickView
 */
MetadataBoxController.prototype.init = function(quickView) {
  this.metadataBox_ = quickView.getFilesMetadataBox();
  this.quickView_ = quickView;

  this.fileMetadataFormatter_.addEventListener(
      'date-time-format-changed', this.updateView_.bind(this));

  this.quickView_.addEventListener(
      'metadata-box-active-changed', this.updateView_.bind(this));

  this.quickViewModel_.addEventListener(
      'selected-entry-changed', this.updateView_.bind(this));

  this.metadataBox_.clear(false);
};

/**
 * @const {!Array<string>}
 */
MetadataBoxController.GENERAL_METADATA_NAME = [
  'size',
  'modificationTime',
];

/**
 * Update the view of metadata box.
 * @param {!Event} event
 * @private
 */
MetadataBoxController.prototype.updateView_ = function(event) {
  if (!this.quickView_.metadataBoxActive) {
    return;
  }

  const entry = this.quickViewModel_.getSelectedEntry();
  const isSameEntry = util.isSameEntry(entry, this.previousEntry_);
  this.previousEntry_ = entry;

  if (!entry) {
    this.metadataBox_.clear(false);
    return;
  }

  if (event.type === 'date-time-format-changed') {
    // Update the displayed entry modificationTime format only, and return.
    this.metadataModel_.get([entry], ['modificationTime'])
        .then(this.updateModificationTime_.bind(this, entry, isSameEntry));
    return;
  }

  // Do not clear isSizeLoading and size fields when the entry is not changed.
  this.metadataBox_.clear(isSameEntry);

  const metadata = MetadataBoxController.GENERAL_METADATA_NAME.concat(
      ['alternateUrl', 'externalFileUrl', 'hosted']);
  this.metadataModel_.get([entry], metadata)
      .then(this.onGeneralMetadataLoaded_.bind(this, entry, isSameEntry));
};

/**
 * Update metadata box with general file information.
 * Then retrieve file specific metadata if any.
 *
 * @param {!Entry} entry
 * @param {boolean} isSameEntry if the entry is not changed from the last time.
 * @param {!Array<!MetadataItem>} items
 *
 * @private
 */
MetadataBoxController.prototype.onGeneralMetadataLoaded_ = function(
    entry, isSameEntry, items) {
  const type = FileType.getType(entry).type;
  const item = items[0];

  if (entry.isDirectory) {
    const directory = /** @type {!DirectoryEntry} */ (entry);
    this.setDirectorySize_(directory, isSameEntry);
  } else if (item.size) {
    this.metadataBox_.size =
        this.fileMetadataFormatter_.formatSize(item.size, item.hosted);
  }

  this.updateModificationTime_(entry, isSameEntry, items);

  if (item.externalFileUrl || item.alternateUrl) {
    this.metadataModel_.get([entry], ['contentMimeType']).then(items => {
      const item = items[0];
      this.metadataBox_.mediaMimeType = item.contentMimeType || '';
    });
  } else {
    this.metadataModel_.get([entry], ['mediaMimeType']).then(items => {
      const item = items[0];
      this.metadataBox_.mediaMimeType = item.mediaMimeType || '';
    });
  }

  this.metadataBox_.type = type;

  if (['image', 'video', 'audio'].includes(type)) {
    if (item.externalFileUrl || item.alternateUrl) {
      this.metadataModel_.get([entry], ['imageHeight', 'imageWidth'])
          .then(items => {
            const item = items[0];
            this.metadataBox_.imageHeight =
                /** @type {number} */ (item.imageHeight);
            this.metadataBox_.imageWidth =
                /** @type {number} */ (item.imageWidth);
          });
    } else {
      this.metadataModel_
          .get(
              [entry],
              [
                'ifd',
                'imageHeight',
                'imageWidth',
                'mediaAlbum',
                'mediaArtist',
                'mediaDuration',
                'mediaGenre',
                'mediaTitle',
                'mediaTrack',
                'mediaYearRecorded',
              ])
          .then(items => {
            const item = items[0];
            this.metadataBox_.ifd = item.ifd || null;
            this.metadataBox_.imageHeight = item.imageHeight || 0;
            this.metadataBox_.imageWidth = item.imageWidth || 0;
            this.metadataBox_.mediaAlbum = item.mediaAlbum || '';
            this.metadataBox_.mediaArtist = item.mediaArtist || '';
            this.metadataBox_.mediaDuration = item.mediaDuration || 0;
            this.metadataBox_.mediaGenre = item.mediaGenre || '';
            this.metadataBox_.mediaTitle = item.mediaTitle || '';
            this.metadataBox_.mediaTrack = item.mediaTrack || '';
            this.metadataBox_.mediaYearRecorded = item.mediaYearRecorded || '';
          });
    }
  }
};

/**
 * Updates the metadata box modificationTime.
 *
 * @param {!Entry} entry
 * @param {boolean} isSameEntry if the entry is not changed from the last time.
 * @param {!Array<!MetadataItem>} items
 *
 * @private
 */
MetadataBoxController.prototype.updateModificationTime_ = function(
    entry, isSameEntry, items) {
  const item = items[0];

  if (item.modificationTime) {
    this.metadataBox_.modificationTime =
        this.fileMetadataFormatter_.formatModDate(item.modificationTime);
  } else {
    this.metadataBox_.modificationTime = '';
  }
};

/**
 * Set a current directory's size in metadata box.
 *
 * A loading animation is shown while fetching the directory size. However, it
 * won't show if there is no size value. Use a dummy value ' ' in that case.
 *
 * If previous getDirectorySize is still running, next getDirectorySize is not
 * called at the time. After the previous callback is finished, getDirectorySize
 * that corresponds to the last setDirectorySize_ is called.
 *
 * @param {!DirectoryEntry} entry
 * @param {boolean} isSameEntry if the entry is not changed from the last time.
 *
 * @private
 */
MetadataBoxController.prototype.setDirectorySize_ = function(
    entry, isSameEntry) {
  if (!entry.isDirectory) {
    return;
  }

  if (this.metadataBox_.size === '') {
    this.metadataBox_.size = ' ';  // Provide a dummy size value.
  }

  if (this.isDirectorySizeLoading_) {
    if (!isSameEntry) {
      this.metadataBox_.isSizeLoading = true;
    }

    // Only retain the last setDirectorySize_ request.
    this.onDirectorySizeLoaded_ = lastEntry => {
      this.setDirectorySize_(entry, util.isSameEntry(entry, lastEntry));
    };
    return;
  }

  // false if the entry is same. true if the entry is changed.
  this.metadataBox_.isSizeLoading = !isSameEntry;

  this.isDirectorySizeLoading_ = true;
  chrome.fileManagerPrivate.getDirectorySize(entry, size => {
    this.isDirectorySizeLoading_ = false;

    if (this.onDirectorySizeLoaded_) {
      setTimeout(this.onDirectorySizeLoaded_.bind(null, entry));
      this.onDirectorySizeLoaded_ = null;
    }

    if (this.quickViewModel_.getSelectedEntry() != entry) {
      return;
    }

    if (chrome.runtime.lastError) {
      this.metadataBox_.isSizeLoading = false;
      return;
    }

    this.metadataBox_.size = this.fileMetadataFormatter_.formatSize(size, true);
    this.metadataBox_.isSizeLoading = false;
  });
};
