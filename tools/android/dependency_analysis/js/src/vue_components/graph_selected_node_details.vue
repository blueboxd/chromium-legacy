<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div class="selected-node-details">
    <template v-if="selectedNode !== null">
      <MdList class="md-double-line">
        <MdListItem>
          <div class="md-list-item-text">
            <span class="selected-node-details-text">
              {{ selectedNode.id }}
            </span>
            <span>Name</span>
          </div>
        </MdListItem>
        <MdListItem>
          <div class="md-list-item-text">
            <span class="selected-node-details-text">
              {{ selectedNode.displayName }}
            </span>
            <span>Display Name</span>
          </div>
        </MdListItem>
      </MdList>
      <MdButton
          v-if="selectedNode.visualizationState.selectedByFilter"
          class="md-primary md-raised md-dense"
          @click="uncheckNodeInFilter">
        Uncheck in filter
      </MdButton>
      <MdButton
          v-else
          class="md-primary md-raised md-dense"
          @click="checkNodeInFilter">
        Add/check in filter
      </MdButton>
    </template>
    <div v-else>
      (Click a node for more details.)
    </div>
  </div>
</template>

<script>
import {CUSTOM_EVENTS} from '../vue_custom_events.js';

// @vue/component
const GraphSelectedNodeDetails = {
  props: {
    selectedNodeDetailsData: Object,
  },
  data: function() {
    return this.selectedNodeDetailsData;
  },
  methods: {
    checkNodeInFilter: function(check) {
      this.$emit(CUSTOM_EVENTS.DETAILS_CHECK_NODE, this.selectedNode.id);
    },
    uncheckNodeInFilter: function(check) {
      this.$emit(CUSTOM_EVENTS.DETAILS_UNCHECK_NODE, this.selectedNode.id);
    },
  },
};

export default GraphSelectedNodeDetails;
</script>

<style scoped>
.selected-node-details {
  display: flex;
  flex-direction: column;
}

.selected-node-details-text{
  display: inline-block;
  white-space: normal;
  width: 100%;
  word-wrap: break-word;
}
</style>
