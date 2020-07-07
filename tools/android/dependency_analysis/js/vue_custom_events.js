// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A collection of names for the custom events to be emitted by the page's
// components (in vue_components/).
const CUSTOM_EVENTS = {
  ADD_TO_FILTER_CLICKED: 'add-to-filter-clicked',
  FILTER_ELEMENT_CLICKED: 'filter-element-clicked',
  FILTER_SUBMITTED: 'filter-submitted',
  INBOUND_DEPTH_UPDATED: 'inbound-depth-updated',
  NODE_CLICKED: 'node-clicked',
  OUTBOUND_DEPTH_UPDATED: 'outbound-depth-updated',
  REMOVE_FROM_FILTER_CLICKED: 'remove-from-filter-clicked',
};

export {
  CUSTOM_EVENTS,
};
