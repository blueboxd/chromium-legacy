<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div class="user-input-group">
    <label for="filter-inbound">Change inbound (blue) depth:</label>
    <input
        id="filter-inbound"
        v-model.number="inboundDepth"
        type="number">
    <button
        type="button"
        @click="submitInbound">
      Update Inbound
    </button>
  </div>
</template>

<script>
import {CUSTOM_EVENTS} from '../vue_custom_events.js';

const GraphInboundInput = {
  props: {
    inboundDepthData: Object,
  },
  data: function() {
    return this.inboundDepthData;
  },
  methods: {
    submitInbound: function() {
      this.$emit(CUSTOM_EVENTS.INBOUND_DEPTH_UPDATED, this.inboundDepth);
    },
  },
};

export default GraphInboundInput;
</script>
