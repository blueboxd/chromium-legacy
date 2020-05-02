// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles Happiness Tracking Surveys for the settings pages. */

// clang-format on
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format off

  /** @interface */
  class HatsBrowserProxy {
    /**
     * Helper function that initiates the launching of HaTS (Happiness Tracking
     * Surveys) through sending a request to HatsService, which is the entity
     * that decides whether it's appropriate to show a survey.
     */
    tryShowSurvey() {}
  }

  /** @implements {HatsBrowserProxy} */
  export class HatsBrowserProxyImpl {
    /** @override*/
    tryShowSurvey() {
      chrome.send('tryShowHatsSurvey');
    }
  }

  addSingletonGetter(HatsBrowserProxyImpl);

