<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <ul class="filter-items">
    <li
        v-for="node in nodeList"
        :key="node.id"
        @click="removeFilter">
      {{ node }}
    </li>
  </ul>
</template>

<script>
import {CUSTOM_EVENTS} from '../vue_custom_events.js';

const GraphFilterItems = {
  props: {
    nodeFilterData: Object,
  },
  data: function() {
    return this.nodeFilterData;
  },
  methods: {
    removeFilter: function(e) {
      const filterText = e.target.textContent;
      this.$emit(CUSTOM_EVENTS.FILTER_ELEMENT_CLICKED, filterText.trim());
    },
  },
};

export default GraphFilterItems;
</script>

<style scoped>
.filter-items {
  overflow: hidden;
  overflow-y: scroll;
  min-width: 100px;
  margin-right: 20px;
}
</style>
