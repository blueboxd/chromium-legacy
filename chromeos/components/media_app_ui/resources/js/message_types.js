// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Message definitions passed over the MediaApp privileged/unprivileged pipe.
 */

/**
 * Enum for message types.
 * @enum {string}
 */
const Message = {
  DELETE_FILE: 'delete-file',
  LOAD_FILES: 'load-files',
  NAVIGATE: 'navigate',
  OPEN_FEEDBACK_DIALOG: 'open-feedback-dialog',
  OVERWRITE_FILE: 'overwrite-file',
  RENAME_FILE: 'rename-file',
  SAVE_COPY: 'save-copy'
};

/**
 * Enum for valid results of deleting a file.
 * @enum {number}
 */
const DeleteResult = {
  SUCCESS: 0,
  FILE_MOVED: 1,
};

/**
 * Message sent by the unprivileged context to request the privileged context to
 * delete the currently writable file.
 * If the supplied file `token` is invalid the request is rejected.
 * @typedef {{token: number}}
 */
let DeleteFileMessage;

/**
 * Response message sent by the privileged context indicating if a requested
 * delete was successful.
 * @typedef {{deleteResult: DeleteResult!}}
 */
let DeleteFileResponse;

/**
 * Message sent by the privileged context to the unprivileged context on launch
 * indicating the initial files available to open.
 * @typedef {{
 *    writableFileIndex: number,
 *    files: !Array<{token: number, file: !File}>
 * }}
 */
let LoadFilesMessage;

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * that the currently writable file be overridden with the provided `blob`.
 * If the supplied file `token` is invalid the request is rejected.
 * @typedef {{token: number, blob: !Blob}}
 */
let OverwriteFileMessage;

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * the app be relaunched with the next/previous file in the current directory
 * set to writable. Direction must be either 'next' or 'prev'.
 * @typedef {{direction: number}}
 */
let NavigateMessage;

/**
 * Enum for valid results of renaming a file.
 * @enum {number}
 */
const RenameResult = {
  SUCCESS: 0,
  FILE_EXISTS: 1,
};

/**
 * Message sent by the unprivileged context to request the privileged context to
 * rename the currently writable file.
 * If the supplied file `token` is invalid the request is rejected.
 * @typedef {{token: number, newFilename: string}}
 */
let RenameFileMessage;

/** @typedef {{renameResult: RenameResult!}}  */
let RenameFileResponse;

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * for the provided file to be copied and saved in a new location which the user
 * is prompted for, i.e a 'save as' operation. Once the native filesystem api
 * allows, this save as dialog will have the filename input be pre-filled with
 * `suggestedName`. `suggestedName` is additionally used to determine the file
 * extension which helps inform the save as dialog as to which files should be
 * overwritable. This method can be called with any file, not just the currently
 * writable file.
 * @typedef {{blob: !Blob, suggestedName: string}}
 */
let SaveCopyMessage;

/**
 * Response message sent by the privileged context indicating the error message
 * if the associated save as operation could not be performed. Returns nothing
 * if the operation was successful.
 * @typedef {{ errorMessage: ?string }}
 */
let SaveCopyResponse;
