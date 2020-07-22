<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div class="user-input-group">
    <label for="filter-outbound">Change outbound (yellow) depth:</label>
    <input
        id="filter-outbound"
        v-model.number="outboundDepthData.outboundDepth"
        type="number">
  </div>
</template>

<script>
// @vue/component
const GraphOutboundInput = {
  props: {
    outboundDepthData: Object,
  },
};

export default GraphOutboundInput;
</script>
