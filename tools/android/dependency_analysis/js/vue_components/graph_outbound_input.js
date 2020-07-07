// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CUSTOM_EVENTS} from '../vue_custom_events.js';

const GraphOutboundInput = {
  props: ['outboundDepthData'],
  data: function() {
    return this.outboundDepthData;
  },
  methods: {
    submitOutbound: function() {
      this.$emit(CUSTOM_EVENTS.OUTBOUND_DEPTH_UPDATED, this.outboundDepth);
    },
  },
  template: `
    <div class="user-input-group">
      <label for="filter-outbound">Change outbound (yellow) depth:</label>
      <input v-model.number="outboundDepth" type="number" id="filter-outbound">
      <button @click="submitOutbound" type="button">Update Outbound</button>
    </div>`,
};

export {
  GraphOutboundInput,
};
