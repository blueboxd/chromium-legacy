#!/bin/bash

# This script renames all the functions and the macros defined in
# absl/base/dynamic_annotations.{h,cc}.
#
# Chromium's dynamic_annotations live in //base/third_party/dynamic_annotations
# which conflict with Abseil's versions (ODR violations).
# In order to avoid problems in Chromium, this copy of Abseil has its own
# dynamic_annotations renamed.

# -------------------------- dynamic_annotations -------------------------
for w in \
  GetRunningOnValgrind \
  RunningOnValgrind \
  ValgrindSlowdown \
; do
  find absl/ -type f -exec sed -i "s/\b$w\b/Absl$w/g" {} \;
done
