// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('[PiexLoader] wasm mode loaded');

/**
 * Declares the piex-wasm Module interface. The Module has many interfaces
 * but only declare the parts required for PIEX work.
 * @typedef {{
 *   calledRun: boolean,
 *   onAbort: function((!Error|string)):undefined,
 *   HEAP8: !Uint8Array,
 *   _malloc: function(number):number,
 *   _free: function(number):undefined,
 *   image: function(number, number):PiexWasmImageResult
 * }}
 */
var PiexWasmModule;

/**
 * |window| var Module defined in page <script src='piex/piex.js.wasm'>.
 * @type {PiexWasmModule}
 */
var Module = window['Module'] || {};

/**
 * Set true only if the wasm Module.onAbort() handler is called.
 * @type {boolean}
 */
let wasmFailed = false;

/**
 * Installs an (Emscripten) wasm Module.onAbort handler, that records that
 * the Module has failed and re-throws the error.
 * @throws {!Error|string}
 */
Module.onAbort = (error) => {
  wasmFailed = true;
  throw error;
};

/**
 * Module failure recovery: if wasmFailed is set via onAbort due to OOM in
 * the C++ for example, or the Module failed to load or call run, then the
 * wasm Module is in a broken, non-functional state.
 *
 * Re-loading the page is the only reliable way to attempt to recover from
 * broken Module state.
 */
function wasmModuleFailed() {
  if (wasmFailed || !Module.calledRun) {
    console.error('[PiexLoader] wasmModuleFailed');
    setTimeout(chrome.runtime.reload, 0);
    return true;
  }
}

/**
 * @param {{id:number, thumbnail:!ArrayBuffer, orientation:number,
 *          colorSpace: ColorSpace, ifd:?string}}
 *     data The data returned from the piex wasm module.
 * @constructor
 * @struct
 */
function PiexLoaderResponse(data) {
  /**
   * @public {number}
   * @const
   */
  this.id = data.id;

  /**
   * @public {!ArrayBuffer}
   * @const
   */
  this.thumbnail = data.thumbnail;

  /**
   * @public {!ImageOrientation}
   * @const
   */
  this.orientation =
      ImageOrientation.fromExifOrientation(data.orientation);

  /**
   * @public {ColorSpace}
   * @const
   */
  this.colorSpace = data.colorSpace;

  /**
   * JSON encoded RAW image photographic details (Piex Wasm module only).
   * @public {?string}
   * @const
   */
  this.ifd = data.ifd || null;
}

/**
 * Creates a PiexLoader for reading RAW image file information.
 * @constructor
 * @struct
 */
function PiexLoader() {
  /**
   * @private {number}
   */
  this.requestIdCount_ = 0;
}

/**
 * Resolves the file entry associated with DOM filesystem |url| and returns
 * the file content in an ArrayBuffer.
 * @param {string} url - DOM filesystem URL of the file.
 * @returns {!Promise<!ArrayBuffer>}
 */
function readFromFileSystem(url) {
  return new Promise((resolve, reject) => {
    /**
     * Reject the Promise on fileEntry URL resolve or file read failures.
     */
    function failure(error) {
      reject(new Error('Reading file system: ' + error));
    }

    /**
     * Returns true if the fileEntry file size is within sensible limits.
     * @param {number} size - file size.
     * @return {boolean}
     */
    function valid(size) {
      return size > 0 && size < Math.pow(2, 30);
    }

    /**
     * Reads the fileEntry's content into an ArrayBuffer: resolve Promise
     * with the ArrayBuffer result or reject the Promise on failure.
     * @param {!Entry} entry - file system entry of |url|.
     */
    function readEntry(entry) {
      const fileEntry = /** @type {!FileEntry} */ (entry);
      fileEntry.file((file) => {
        if (valid(file.size)) {
          const reader = new FileReader();
          reader.onerror = failure;
          reader.onload = (_) => resolve(reader.result);
          reader.readAsArrayBuffer(file);
        } else {
          failure('invalid file size: ' + file.size);
        }
      }, failure);
    }

    window.webkitResolveLocalFileSystemURL(url, readEntry, failure);
  });
}

/**
 * Piex wasm extacts the preview image metadata from a raw image. The preview
 * image |format| is either 0 (JPEG) or 1 (RGB), and has a |colorSpace| (sRGB
 * or AdobeRGB1998) and a JEITA EXIF image |orientation|.
 *
 * An RGB format preview image has both |width| and |height|, but JPEG format
 * previews have neither (piex wasm C++ does not parse/decode JPEG).
 *
 * The |offset| to, and |length| of, the preview image relative to the source
 * data is indicated by those fields. They are positive > 0. Note: the values
 * are controlled by a third-party and are untrustworthy (Security).
 *
 * @typedef {{
 *  format:number,
 *  colorSpace:ColorSpace,
 *  orientation:number,
 *  width:?number,
 *  height:?number,
 *  offset:number,
 *  length:number
 * }}
 */
var PiexWasmPreviewImageMetadata;

/**
 * The piex wasm Module.image(<raw image source>,...) API returns |error|, or
 * else the source |preview| and/or |thumbnail| image metadata along with the
 * photographic |details| derived from the RAW image EXIF.
 *
 * FilesApp (and related) only use |preview| images. Preview images are JPEG.
 * The |thumbnail| images are small, lower-quality, JPEG or RGB format images
 * and are not currently used in FilesApp.
 *
 * @typedef {{
 *  error:?string,
 *  preview:?PiexWasmPreviewImageMetadata,
 *  thumbnail:?PiexWasmPreviewImageMetadata,
 *  details:?Object
 * }}
 */
var PiexWasmImageResult;

/**
 * Piex wasm raw image preview image extractor.
 */
class ImageBuffer {
  /**
   * @param {!ArrayBuffer} buffer - raw image source data.
   * @param {number} id - caller-defined id.
   */
  constructor(buffer, id) {
    /**
     * @type {number}
     * @const
     * @private
     */
    this.id = id;

    /**
     * @type {!Uint8Array}
     * @const
     * @private
     */
    this.source = new Uint8Array(buffer);

    /**
     * @type {number}
     * @const
     * @private
     */
    this.length = buffer.byteLength;

    /**
     * @type {number}
     * @private
     */
    this.memory = 0;
  }

  /**
   * Calls Module.image() to process |this.source| and return the result.
   *
   * @return {!PiexWasmImageResult}
   * @throws {!Error}
   */
  process() {
    this.memory = Module._malloc(this.length);
    if (!this.memory) {
      throw new Error('Image malloc failed: ' + this.length + ' bytes');
    }

    Module.HEAP8.set(this.source, this.memory);
    const result = Module.image(this.memory, this.length);
    if (result.error) {
      throw new Error(result.error);
    }

    return result;
  }

  /**
   * Returns the preview image data. If no preview image was found, returns
   * an empty preview image.
   *
   * @param {!PiexWasmImageResult} result
   *
   * @throws {!Error} Data access security error.
   *
   * @return {{id:number, thumbnail:!ArrayBuffer, orientation:number,
   *          colorSpace: ColorSpace, ifd:?string}}
   */
  preview(result) {
    const preview = result.preview;
    if (!preview) {
      return {
        thumbnail: new ArrayBuffer(0),
        colorSpace: ColorSpace.SRGB,
        orientation: 1,
        id: this.id,
        ifd: null,
      };
    }

    const offset = preview.offset;
    const length = preview.length;
    if (offset > this.length || (this.length - offset) < length) {
      throw new Error('Preview image access failed');
    }

    const view = new Uint8Array(this.source.buffer, offset, length);
    return {
      thumbnail: new Uint8Array(view).buffer,
      orientation: preview.orientation,
      colorSpace: preview.colorSpace,
      ifd: this.details(result),
      id: this.id,
    };
  }

  /**
   * Returns the RAW image photographic |details| in a JSON-encoded string.
   * Only number and string values are retained, and they are formatted for
   * presentation to the user.
   *
   * @private
   * @param {!PiexWasmImageResult} result
   * @return {?string}
   */
  details(result) {
    const details = result.details;
    if (!details) {
      return null;
    }

    let format = {};
    for (const [key, value] of Object.entries(details)) {
      if (typeof value === 'string') {
        format[key] = value.replace(/\0+$/, '').trim();
      } else if (typeof value === 'number') {
        if (!Number.isInteger(value)) {
          format[key] = Number(value.toFixed(3).replace(/0+$/, ''));
        } else {
          format[key] = value;
        }
      }
    }

    return JSON.stringify(format);
  }

  /**
   * Release resources.
   */
  close() {
    Module._free(this.memory);
  }
}

/**
 * Starts to load RAW image.
 * @param {string} url
 * @return {!Promise<!PiexLoaderResponse>}
 */
PiexLoader.prototype.load = function(url) {
  const requestId = this.requestIdCount_++;

  let imageBuffer;
  return readFromFileSystem(url)
      .then((buffer) => {
        if (wasmModuleFailed() === true) {
          return Promise.reject('piex wasm module failed');
        }
        imageBuffer = new ImageBuffer(buffer, requestId);
        return imageBuffer.process();
      })
      .then((result) => {
        imageBuffer.close();
        return new PiexLoaderResponse(imageBuffer.preview(result));
      })
      .catch((error) => {
        if (wasmModuleFailed() === true) {
          return Promise.reject('piex wasm module failed');
        }
        imageBuffer && imageBuffer.close();
        console.error('[PiexLoader] ' + error);
        return Promise.reject(error);
      });
};
