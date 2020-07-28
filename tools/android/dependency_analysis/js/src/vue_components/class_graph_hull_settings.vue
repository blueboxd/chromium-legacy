<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div id="hull-settings">
    <label>Convex hull display:</label>
    <div
        v-for="hullDisplay in HullDisplay"
        :key="hullDisplay">
      <input
          :id="hullDisplay"
          v-model="internalSelectedHullDisplay"
          type="radio"
          name="hullDisplayRadioButtons"
          :value="hullDisplay"
          @change="updateSelectedHullDisplay">
      <label :for="hullDisplay">
        {{ hullDisplay }}
      </label>
    </div>
  </div>
</template>

<script>
import {HullDisplay} from '../class_view_consts.js';

// @vue/component
const ClassGraphHullSettings = {
  props: {
    selectedHullDisplay: String,
  },
  data: function() {
    return {
      internalSelectedHullDisplay: this.selectedHullDisplay,
    };
  },
  computed: {
    HullDisplay: () => HullDisplay,
  },
  methods: {
    updateSelectedHullDisplay: function() {
      this.$emit(
          'update:selectedHullDisplay', this.internalSelectedHullDisplay);
    },
  },
};

export default ClassGraphHullSettings;
</script>

<style scoped>
#hull-settings {
  display: flex;
  flex-direction: column;
  margin-bottom: 10px;
}
</style>
