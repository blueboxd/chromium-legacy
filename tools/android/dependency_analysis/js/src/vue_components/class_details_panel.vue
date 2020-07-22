<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div
      v-if="selectedClass !== null"
      class="class-details-panel">
    <LinkToGraph
        :filter="[selectedClass.packageName]"
        :graph-type="PagePathName.PACKAGE"
        :text="'View ' + selectedClass.packageName"/>
    <div v-if="selectedClass.buildTargets.length > 0">
      <div>Class buildtarget(s):</div>
      <ul>
        <li
            v-for="buildTarget in selectedClass.buildTargets"
            :key="buildTarget">
          {{ buildTarget }}
        </li>
      </ul>
    </div>
  </div>
</template>

<script>
import {PagePathName} from '../url_processor.js';

import LinkToGraph from './link_to_graph.vue';

// @vue/component
const ClassDetailsPanel = {
  components: {
    LinkToGraph,
  },
  props: {
    selectedClass: Object,
  },
  computed: {
    PagePathName: () => PagePathName,
  },
};

export default ClassDetailsPanel;
</script>
