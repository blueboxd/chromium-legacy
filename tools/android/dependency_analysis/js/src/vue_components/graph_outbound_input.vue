<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div class="user-input-group">
    <label for="filter-outbound">Change outbound (yellow) depth:</label>
    <input
        id="filter-outbound"
        v-model.number="outboundDepth"
        type="number">
    <button
        type="button"
        @click="submitOutbound">
      Update Outbound
    </button>
  </div>
</template>

<script>
import {CUSTOM_EVENTS} from '../vue_custom_events.js';

const GraphOutboundInput = {
  props: {
    outboundDepthData: Object,
  },
  data: function() {
    return this.outboundDepthData;
  },
  methods: {
    submitOutbound: function() {
      this.$emit(CUSTOM_EVENTS.OUTBOUND_DEPTH_UPDATED, this.outboundDepth);
    },
  },
};

export default GraphOutboundInput;
</script>
