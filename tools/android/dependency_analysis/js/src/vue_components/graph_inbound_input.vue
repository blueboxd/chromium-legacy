<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div class="user-input-group">
    <label for="filter-inbound">Change inbound (blue) depth:</label>
    <input
        id="filter-inbound"
        v-model.number="inboundDepthData.inboundDepth"
        type="number">
  </div>
</template>

<script>
// @vue/component
const GraphInboundInput = {
  props: {
    inboundDepthData: Object,
  },
};

export default GraphInboundInput;
</script>
