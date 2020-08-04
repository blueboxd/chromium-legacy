// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Registers all NTP modules given their respective descriptors.
 */

import {dummyDescriptor} from './dummy/module.js';
import {ModuleRegistry} from './module_registry.js';

/** @type {!ModuleRegistry} */
export const registry = new ModuleRegistry([dummyDescriptor]);
