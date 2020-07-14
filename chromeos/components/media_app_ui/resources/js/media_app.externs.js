// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview @externs
 * Externs file shipped into the chromium build to typecheck uncompiled, "pure"
 * JavaScript used to interoperate with the open-source privileged WebUI.
 * TODO(b/142750452): Convert this file to ES6.
 */

/**
 * Response message to a successful overwrite (no error thrown). If defined,
 * indicates that an overwrite failed, but the user was able to select a new
 * file from a file picker. The UI should update to reflect the new name.
 * `errorName` is the error on the write attempt that triggered the picker.
 * @typedef {{renamedTo: string, errorName: string}|undefined}
 */
let OverwriteFileResponse;

/** @const */
const mediaApp = {};

/**
 * Wraps an HTML File object (or a mock, or media loaded through another means).
 * @record
 * @struct
 */
mediaApp.AbstractFile = function() {};
/**
 * The native Blob representation.
 * @type {!Blob}
 */
mediaApp.AbstractFile.prototype.blob;
/**
 * A name to represent this file in the UI. Usually the filename.
 * @type {string}
 */
mediaApp.AbstractFile.prototype.name;
/**
 * Size of the file, e.g., from the HTML5 File API.
 * @type {number}
 */
mediaApp.AbstractFile.prototype.size;
/**
 * Mime Type of the file, e.g., from the HTML5 File API. Note that the name
 * intentionally does not match the File API version because 'type' is a
 * reserved word in TypeScript.
 * @type {string}
 */
mediaApp.AbstractFile.prototype.mimeType;
/**
 * Whether the file came from the clipboard or a similar in-memory source not
 * backed by a file on disk.
 * @type {boolean|undefined}
 */
mediaApp.AbstractFile.prototype.fromClipboard;
/**
 * An error associated with this file.
 * @type {string|undefined}
 */
mediaApp.AbstractFile.prototype.error;
/**
 * A function that will overwrite the original file with the provided Blob.
 * Returns a promise that resolves when the write operations are complete. Or
 * rejects. Upon success, `size` will reflect the new file size.
 * If null, then in-place overwriting is not supported for this file.
 * Note the "overwrite" may be simulated with a download operation.
 * @type {function(!Blob): !Promise<!OverwriteFileResponse>}
 */
mediaApp.AbstractFile.prototype.overwriteOriginal;
/**
 * A function that will delete the original file. Returns a promise that
 * resolves to an enum value (see DeleteResult in chromium message_types)
 * reflecting the result of the deletion. Errors encountered are thrown from the
 * message pipe and handled by invoking functions in Google3.
 * @type {function(): !Promise<number>|undefined}
 */
mediaApp.AbstractFile.prototype.deleteOriginalFile;
/**
 * A function that will rename the original file. Returns a promise that
 * resolves to an enum value (see RenameResult in message_types) reflecting the
 * result of the rename. Errors encountered are thrown from the message pipe and
 * handled by invoking functions in Google3.
 * @type {function(string): !Promise<number>|undefined}
 */
mediaApp.AbstractFile.prototype.renameOriginalFile;

/**
 * Wraps an HTML FileList object.
 * @record
 * @struct
 */
mediaApp.AbstractFileList = function() {};
/** @type {number} */
mediaApp.AbstractFileList.prototype.length;
/**
 * @param {number} index
 * @return {(null|!mediaApp.AbstractFile)}
 */
mediaApp.AbstractFileList.prototype.item = function(index) {};
/**
 * Returns the file which is currently writable or null if there isn't one.
 * @return {?mediaApp.AbstractFile}
 */
mediaApp.AbstractFileList.prototype.getCurrentlyWritable = function() {};
/**
 * Loads in the next file in the list as a writable.
 * @return {!Promise<undefined>}
 */
mediaApp.AbstractFileList.prototype.loadNext = function() {};
/**
 * Loads in the previous file in the list as a writable.
 * @return {!Promise<undefined>}
 */
mediaApp.AbstractFileList.prototype.loadPrev = function() {};
/**
 * @param {function(!mediaApp.AbstractFileList): void} observer invoked when the
 *     size or contents of the file list changes.
 */
mediaApp.AbstractFileList.prototype.addObserver = function(observer) {};

/**
 * The delegate which exposes open source privileged WebUi functions to
 * MediaApp.
 * @record
 * @struct
 */
mediaApp.ClientApiDelegate = function() {};
/**
 * Opens up the built-in chrome feedback dialog.
 * @return {!Promise<?string>} Promise which resolves when the request has been
 *     acknowledged, if the dialog could not be opened the promise resolves with
 *     an error message, resolves with null otherwise.
 */
mediaApp.ClientApiDelegate.prototype.openFeedbackDialog = function() {};
/**
 * Request for the user to be prompted with a save file dialog. Once the user
 * selects a location a new file handle is created and a unique token to that
 * file will be returned. This token can be then used with saveCopy(). The file
 * extension on `suggestedName` and the provided `mimeType` are used to inform
 * the save as dialog what file should be created. Once the Native Filesystem
 * API allows, this save as dialog will additionally have the filename input be
 * pre-filled with `suggestedName`.
 * TODO(b/161087799): Update function description once Native Filesystem API
 * supports suggestedName.
 * @param {string} suggestedName
 * @param {string} mimeType
 * @return {!Promise<number>}
 */
mediaApp.ClientApiDelegate.prototype.requestSaveFile = function(
    suggestedName, mimeType) {};
/**
 * Saves a copy of `file` in the file specified by `token`.
 * @param {!mediaApp.AbstractFile} file
 * @param {number} token
 * @return {!Promise<undefined>}
 */
mediaApp.ClientApiDelegate.prototype.saveCopy = function(file, token) {};

/**
 * The client Api for interacting with the media app instance.
 * @record
 * @struct
 */
mediaApp.ClientApi = function() {};
/**
 * Looks up handler(s) and loads media via FileList.
 * @param {!mediaApp.AbstractFileList} files
 * @return {!Promise<undefined>}
 */
mediaApp.ClientApi.prototype.loadFiles = function(files) {};
/**
 * Sets the delegate through which MediaApp can access open-source privileged
 * WebUI methods.
 * @param {?mediaApp.ClientApiDelegate} delegate
 */
mediaApp.ClientApi.prototype.setDelegate = function(delegate) {};

/**
 * Launch data that can be read by the app when it first loads.
 * @type {{files: mediaApp.AbstractFileList}}
 */
window.customLaunchData;
