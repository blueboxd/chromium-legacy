// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './promo_browser_command.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending the NTP promos browser commands to the browser and
 * receiving the browser response.
 */

export class PromoBrowserCommandProxy {
  constructor() {
    /** @type {!promoBrowserCommand.mojom.CommandHandlerRemote} */
    this.handler = promoBrowserCommand.mojom.CommandHandler.getRemote();
  }
}

addSingletonGetter(PromoBrowserCommandProxy);
