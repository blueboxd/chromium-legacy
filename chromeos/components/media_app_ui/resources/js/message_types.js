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
  LOAD_FILE: 'load-file',
  OVERWRITE_FILE: 'overwrite-file',
};

/** @typedef {{token: number, file: !File}} */
let OpenFileMessage;

/** @typedef {{token: number, blob: !Blob}} */
let OverwriteFileMessage;
