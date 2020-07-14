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
  IFRAME_READY: 'iframe-ready',
  LOAD_EXTRA_FILES: 'load-extra-files',
  LOAD_FILES: 'load-files',
  NAVIGATE: 'navigate',
  OPEN_FEEDBACK_DIALOG: 'open-feedback-dialog',
  OVERWRITE_FILE: 'overwrite-file',
  RENAME_FILE: 'rename-file',
  REQUEST_SAVE_FILE: 'request-save-file',
  SAVE_COPY: 'save-copy'
};

/**
 * Enum for results of deleting a file.
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
 * Representation of a file passed in on the LoadFilesMessage.
 * @typedef {{token: number, file: ?File, name: string, error: string}}
 */
let FileContext;

/**
 * Message sent by the privileged context to the unprivileged context indicating
 * the files available to open.
 * @typedef {{
 *    writableFileIndex: number,
 *    files: !Array<!FileContext>
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
 * Enum for results of renaming a file.
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
 * for the user to be prompted with a save file dialog. Once the user selects a
 * location a new file handle is created and a unique token to that file
 * will be returned. This token can be then used with saveCopy(). The file
 * extension on `suggestedName` and the provided `mimeType` are used to inform
 * the save as dialog what file should be created. Once the native filesystem
 * api allows, this save as dialog will additionally have the filename input be
 * pre-filled with `suggestedName`.
 * @typedef {{suggestedName: string, mimeType: string}}
 */
let RequestSaveFileMessage;

/**
 * Response message sent by the privileged context with a unique identifier for
 * the new writable file created on disk by the corresponding request save
 * file message.
 * @typedef {{token: number}}
 */
let RequestSaveFileResponse;

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * for the provided file to be copied and saved in the location specified by
 * `token`. This method can be called with any file, not just the currently
 * writable file.
 * @typedef {{blob: !Blob, token: number}}
 */
let SaveCopyMessage;
