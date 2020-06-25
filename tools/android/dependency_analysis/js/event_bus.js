// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const eventBus = new Vue();
const eventBusEvents = {
  D3_NODE_CLICKED: 'd3-node-clicked',
};

export {
  eventBus,
  eventBusEvents,
};
