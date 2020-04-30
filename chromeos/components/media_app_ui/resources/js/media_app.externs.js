// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview @externs
 * Externs file shipped into the chromium build to typecheck uncompiled, "pure"
 * JavaScript used to interoperate with the open-source privileged WebUI.
 * TODO(b/142750452): Convert this file to ES6.
 */

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
 * A function that will overwrite the original file with the provided Blob.
 * Returns a promise that resolves when the write operations are complete. Or
 * rejects. Upon success, `size` will reflect the new file size.
 * If null, then in-place overwriting is not supported for this file.
 * Note the "overwrite" may be simulated with a download operation.
 * @type {function(!Blob): Promise<undefined>|undefined}
 */
mediaApp.AbstractFile.prototype.overwriteOriginal;
/**
 * A function that will delete the original file. Returns a promise that
 * resolves to an enum value (see DeleteResult in chromium message_types)
 * reflecting the result of the deletion (SUCCESS, FILE_MOVED). Rejected if an
 * error is thrown.
 * @type {function(): Promise<number>|undefined}
 */
mediaApp.AbstractFile.prototype.deleteOriginalFile;
/**
 * A function that will rename the original file. Returns a promise that
 * resolves to an enum value (see RenameResult in message_types) reflecting the
 * result of the rename (SUCCESS, FILE_EXISTS). Rejected if an error is thrown.
 * @type {function(string): Promise<number>|undefined}
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
 * Saves a copy of `file` in a custom location with a custom
 * name which the user is prompted for via a native save file dialog.
 * @param {!mediaApp.AbstractFile} file
 * @return {!Promise<?string>} Promise which resolves when the request has been
 *     acknowledged. If the dialog could not be opened the promise resolves with
 *     an error message. Otherwise, with null after writing is complete.
 */
mediaApp.ClientApiDelegate.prototype.saveCopy = function(file) {};

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
 * The message structure sent to the guest over postMessage. The presence of
 * a particular field determines the instruction being given to the guest.
 *
 * @record
 * @struct
 */
mediaApp.MessageEventData = function() {};
/**
 * File data to load. TODO(b/144865801): Remove this (obsolete).
 *
 * @type {!ArrayBuffer|undefined}
 */
mediaApp.MessageEventData.prototype.buffer;
/**
 * MIME type of the data in `buffer`.
 *
 * @type {string|undefined}
 */
mediaApp.MessageEventData.prototype.type;
/**
 * An object that uniquely identifies a FileSystemFileHandle in the host.
 *
 * @type {!Object|undefined}
 */
mediaApp.MessageEventData.prototype.handle;
/**
 * A File to load.
 *
 * @type {!File|undefined}
 */
mediaApp.MessageEventData.prototype.file;

/**
 * Launch data that can be read by the app when it first loads.
 * @type {{files: mediaApp.AbstractFileList}}
 */
window.customLaunchData;
