// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * `installed-app-checkbox` is a checkbox that displays an installed app.
 * An installed app could be a domain with data that the user might want
 * to protect from being deleted.
 */
Polymer({
  is: 'installed-app-checkbox',

  properties: {
    /** @type {InstalledApp} */
    installed_app: Object,
    disabled: {
      type: Boolean,
      value: false,
    },
  },
});
