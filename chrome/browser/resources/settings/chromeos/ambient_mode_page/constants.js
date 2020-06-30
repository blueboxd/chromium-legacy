// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enumeration of the topic source, i.e. where the photos come from.
 * Values need to stay in sync with the enum |ash::AmbientModeTopicSource|.
 * @enum {number}
 */
/* #export */ const AmbientModeTopicSource = {
  UNKNOWN: -1,
  GOOGLE_PHOTOS: 0,
  ART_GALLERY: 1,
};

/**
 * Settings containing topic source and the photos containers.
 *
 * @typedef {{
 *   topicSource: !AmbientModeTopicSource,
 *   topicContainers: !Array,
 * }}
 */
/* #export */ let AmbientModeSettings;
