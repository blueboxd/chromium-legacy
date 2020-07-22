// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview @externs
 * Externs for draft interfaces not yet upstreamed to closure.
 */

/** @type {function(): !Promise<!ArrayBuffer>} */
Blob.prototype.arrayBuffer;

/**
 * @see https://www.w3.org/TR/2016/WD-html51-20160310/webappapis.html#the-promiserejectionevent-interface
 * @extends {Event}
 * @constructor
 */
const PromiseRejectionEvent = function() {};

/** @type {Promise<*>} */
PromiseRejectionEvent.prototype.promise;

/** @type {*} */
PromiseRejectionEvent.prototype.reason;
