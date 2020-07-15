<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <a :href="url">{{ text }}</a>
</template>

<script>
import {generateUrlFromFilter} from '../url_processor.js';

const LinkToGraph = {
  props: {
    filter: Array,
    graphType: String,
    text: String,
  },
  computed: {
    url: function() {
      return generateUrlFromFilter(document.URL, this.graphType, this.filter);
    },
  },
};

export default LinkToGraph;
</script>
