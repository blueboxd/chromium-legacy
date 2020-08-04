// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides the module descriptor type declaration. Each module
 * must create a module descriptor and register it at the NTP.
 */

/**
 * @typedef {{
 *   id: string,
 *   create: function(): !Promise<!HTMLElement>,
 * }}
 */
export let ModuleDescriptor;
