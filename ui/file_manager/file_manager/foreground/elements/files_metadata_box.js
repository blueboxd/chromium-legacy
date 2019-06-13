// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var FilesMetadataBox = Polymer({
  is: 'files-metadata-box',

  properties: {
    // File media type, e.g. image, video.
    type: String,
    size: String,
    modificationTime: String,
    filePath: String,
    mediaMimeType: String,

    // File type specific metadata below.
    imageWidth: {
      type: Number,
      observer: 'metadataUpdated_',
    },
    imageHeight: {
      type: Number,
      observer: 'metadataUpdated_',
    },
    mediaAlbum: {
      type: String,
      observer: 'metadataUpdated_',
    },
    mediaArtist: {
      type: String,
      observer: 'metadataUpdated_',
    },
    mediaDuration: {
      type: Number,
      observer: 'metadataUpdated_',
    },
    mediaGenre: {
      type: String,
      observer: 'metadataUpdated_',
    },
    mediaTitle: {
      type: String,
      observer: 'metadataUpdated_',
    },
    mediaTrack: {
      type: String,
      observer: 'metadataUpdated_',
    },
    mediaYearRecorded: {
      type: String,
      observer: 'metadataUpdated_',
    },

    /**
     * Exif parsed by exif_parser.js or extracted from RAW files, or null if
     * there is no Exif information. For RAW files, the Exif data resides in
     * the object |ifd.raw| field.
     * @type {?Object}
     */
    ifd: {
      type: Object,
      observer: 'metadataUpdated_',
    },

    // Whether the size is the middle of loading.
    isSizeLoading: Boolean,

    /**
     * @private
     */
    hasFileSpecificInfo_: Boolean,
  },

  /**
   * Clears fields.
   * @param {boolean} keepSizeFields do not clear size and isSizeLoading fields.
   */
  clear: function(keepSizeFields) {
    this.type = '';
    this.modificationTime = '';
    this.mediaMimeType = '';
    this.filePath = '';

    this.imageWidth = 0;
    this.imageHeight = 0;
    this.mediaTitle = '';
    this.mediaArtist = '';
    this.mediaAlbum = '';
    this.mediaDuration = 0;
    this.mediaGenre = '';
    this.mediaTrack = '';
    this.mediaYearRecorded = '';
    this.ifd = null;

    if (!keepSizeFields) {
      this.size = '';
      this.isSizeLoading = false;
    }
  },

  /**
   * @param {string} type
   * @return {boolean}
   *
   * @private
   */
  isImage_: function(type) {
    return type === 'image';
  },

  /**
   * @param {string} type
   * @return {boolean}
   *
   * @private
   */
  isVideo_: function(type) {
    return type === 'video';
  },

  /**
   * @param {string} type
   * @return {boolean}
   *
   * @private
   */
  isAudio_: function(type) {
    return type === 'audio';
  },

  /**
   * Update private properties computed from metadata.
   * @private
   */
  metadataUpdated_: function() {
    this.hasFileSpecificInfo_ =
        !!(this.imageWidth && this.imageHeight || this.mediaTitle ||
           this.mediaArtist || this.mediaAlbum || this.mediaDuration ||
           this.mediaGenre || this.mediaTrack || this.mediaYearRecorded ||
           this.ifd);
  },

  /**
   * Converts the duration into human friendly string.
   * @param {number} time the duration in seconds.
   * @return {string} String representation of the given duration.
   */
  time2string_: function(time) {
    if (!time) {
      return '';
    }

    time = parseInt(time, 10);
    const seconds = time % 60;
    const minutes = Math.floor(time / 60) % 60;
    const hours = Math.floor(time / 60 / 60);

    if (hours === 0) {
      return minutes + ':' + ('0' + seconds).slice(-2);
    }
    return hours + ':' + ('0' + minutes).slice(-2) + ('0' + seconds).slice(-2);
  },

  /**
   * @param {number} imageWidth
   * @param {number} imageHeight
   * @return {string}
   *
   * @private
   */
  dimension_: function(imageWidth, imageHeight) {
    if (imageWidth && imageHeight) {
      return imageWidth + ' x ' + imageHeight;
    }
    return '';
  },

  /**
   * @param {?Object} ifd
   * @return {string}
   *
   * @private
   */
  deviceModel_: function(ifd) {
    if (!ifd) {
      return '';
    }

    // TODO(noel): add ifd.raw cameraModel support.

    const id = 272;
    const model = (ifd.image && ifd.image[id] && ifd.image[id].value) || '';
    return model.replace(/\0+$/, '').trim();
  },

  /**
   * @param {Array<String>} r array of two strings representing the numerator
   *     and the denominator.
   * @return {number}
   * @private
   */
  parseRational_: function(r) {
    const num = parseInt(r[0], 10);
    const den = parseInt(r[1], 10);
    return num / den;
  },

  /**
   * Returns geolocation as a string in the form of 'latitude, longitude',
   * where the values have 3 decimal precision. Negative latitude indicates
   * south latitude and negative longitude indicates west longitude.
   * @param {?Object} ifd
   * @return {string}
   *
   * @private
   */
  geography_: function(ifd) {
    const gps = ifd && ifd.gps;
    if (!gps || !gps[1] || !gps[2] || !gps[3] || !gps[4]) {
      return '';
    }

    const computeCoordinate = value => {
      return this.parseRational_(value[0]) +
          this.parseRational_(value[1]) / 60 +
          this.parseRational_(value[2]) / 3600;
    };

    const latitude =
        computeCoordinate(gps[2].value) * (gps[1].value === 'N\0' ? 1 : -1);
    const longitude =
        computeCoordinate(gps[4].value) * (gps[3].value === 'E\0' ? 1 : -1);

    return Number(latitude).toFixed(3) + ', ' + Number(longitude).toFixed(3);
  },

  /**
   * Returns device settings as a string containing the fNumber, exposureTime,
   * focalLength, and isoSpeed. Example: 'f/2.8 0.008 23mm ISO4000'.
   * @param {?Object} ifd
   * @return {string}
   *
   * @private
   */
  deviceSettings_: function(ifd) {
    let result = '';

    // TODO(noel): add ifd.raw device settings support.

    if (ifd) {
      result = this.ifdDeviceSettings_(ifd);
    }

    return result;
  },

  /**
   * @param {Object} ifd
   * @return {string}
   *
   * @private
   */
  ifdDeviceSettings_: function(ifd) {
    const exif = ifd['exif'];
    if (!exif) {
      return '';
    }

    /**
     * @param {Object} field Exif field to be parsed as a number.
     * @return {number}
     */
    function parseExifNumber(field) {
      let number = 0;

      if (field && field.value) {
        if (Array.isArray(field.value)) {
          const denominator = parseInt(field.value[1], 10);
          if (denominator) {
            number = parseInt(field.value[0], 10) / denominator;
          }
        } else {
          number = parseInt(field.value, 10);
        }

        if (Number.isNaN(number)) {
          number = 0;
        } else if (!Number.isInteger(number)) {
          number = Number(number.toFixed(3).replace(/0+$/, ''));
        }
      }

      return number;
    }

    let result = '';

    const aperture = parseExifNumber(exif[33437]);
    if (aperture) {
      result += 'f/' + aperture + ' ';
    }

    const exposureTime = parseExifNumber(exif[33434]);
    if (exposureTime) {
      result += exposureTime + ' ';
    }

    const focalLength = parseExifNumber(exif[37386]);
    if (focalLength) {
      result += focalLength + 'mm ';
    }

    const isoSpeed = parseExifNumber(exif[34855]);
    if (isoSpeed) {
      result += 'ISO' + isoSpeed + ' ';
    }

    return result.trimEnd();
  },
});
