// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Must be kept in sync with the C++ ScalingType enum in
   * printing/print_job_constants.h.
   * @enum {number}
   */
  const ScalingType = {
    DEFAULT: 0,
    FIT_TO_PAGE: 1,
    FIT_TO_PAPER: 2,
    CUSTOM: 3,
  };

  // Export
  return {
    ScalingType: ScalingType,
  };
});
