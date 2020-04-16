#!/bin/bash
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This tool rolls:
#   - The latest webgpu-cts master branch into external/wpt/
#     (which is auto-upstreamed to WPT)
#   - The latest webgpu-cts glsl-dependent branch into wpt_internal/
# It does the following for each branch:
#   - Updates Chromium's DEPS to the latest origin/{master,glsl-dependent}.
#   - Runs gclient sync.
#   - Builds the CTS (requires a local installation of node/npm + yarn).
#   - Copies the built out-wpt/ directory into
#     {external/wpt,wpt_internal}/webgpu/.
#   - Adds {external/wpt,wpt_internal}/webgpu/ to the git index
#     (so that it doesn't drown out other changes).
#
# It does NOT regenerate the wpt_internal/webgpu/cts.html file, which is used
# by Chromium's automated testing. Note the alert at the end of this script.

set -e

cd "$(dirname "$0")"/../../../..  # cd to [chromium]/src/

roll_cts_to() {
  target_ref=$1

  pushd third_party/webgpu-cts/src > /dev/null

    if ! git diff-index --quiet HEAD ; then
      echo 'third_party/webgpu-cts/src must be clean'
      exit 1
    fi

    git fetch origin
    hash=$(git show-ref --hash "$target_ref")

    echo
    echo "** Rolling to ${target_ref}: **"
    echo
    git log -1 "${target_ref}"
    echo

  popd > /dev/null

  perl -pi -e "s:gpuweb/cts.git' \+ '\@' \+ '[0-9a-f]{40}',$:gpuweb/cts.git' + '\@' + '${hash}',:" DEPS
  gclient sync


  pushd third_party/webgpu-cts/src > /dev/null

    yarn install --frozen-lockfile
    npx grunt wpt  # build third_party/webgpu-cts/src/out-wpt/

  popd > /dev/null
}

roll_cts_to origin/master
rsync -au --del --exclude='/OWNERS' \
  third_party/webgpu-cts/src/out-wpt/ \
  third_party/blink/web_tests/external/wpt/webgpu/
git add third_party/blink/web_tests/external/wpt/webgpu/

roll_cts_to origin/glsl-dependent
rsync -au --del --exclude='/OWNERS' \
  --exclude '/cts.html' --exclude '/third_party' \
  third_party/webgpu-cts/src/out-wpt/ \
  third_party/blink/web_tests/wpt_internal/webgpu/
git add third_party/blink/web_tests/wpt_internal/webgpu/

cat << EOF

********************************************************************
Roll complete!

Remember to run ./regenerate_internal_cts_html.sh.
Updates to WebGPUExpectations may be necessary to make it succeed.

Further updates to WebGPUExpectations may be needed to pass the
Chromium CQ. Note: If a small part of a large test file is failing,
consider suppressing just that part and rerunning
./regenerate_internal_cts_html.sh, which will break down the tests
into small enough chunks to allow the expectations to be applied.
********************************************************************
EOF
