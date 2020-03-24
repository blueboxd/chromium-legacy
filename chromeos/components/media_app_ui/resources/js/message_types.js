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
  LOAD_FILE: 'load-file',
  OPEN_FEEDBACK_DIALOG: 'open-feedback-dialog',
  OVERWRITE_FILE: 'overwrite-file',
};

/**
 * Enum for valid results of deleting a file.
 * @enum {number}
 */
const DeleteResult = {
  SUCCESS: 0,
  FILE_MOVED: 1,
};

/** @typedef {{ token: number }} */
let DeleteFileMessage;

/** @typedef {{ deleteResult: DeleteResult }}  */
let DeleteFileResponse;

/** @typedef {{token: number, file: !File}} */
let OpenFileMessage;

/** @typedef {{token: number, blob: !Blob}} */
let OverwriteFileMessage;
