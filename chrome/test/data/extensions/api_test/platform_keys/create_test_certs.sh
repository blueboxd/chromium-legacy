#!/bin/bash

# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates the following tree of certificates:
#     root (self-signed root)
#     \   \
#      \   \--> l1_leaf (end-entity)
#       \
#        \----> l1_interm --> l2_leaf (end-entity)

SRC_DIR="../../../../../.."
export CA_CERT_UTIL_DIR="${SRC_DIR}/chrome/test/data/policy/ca_util"
source "${CA_CERT_UTIL_DIR}/ca_util.sh"
export CA_CERT_UTIL_OUT_DIR="./out/"

try rm -rf out
try mkdir out

CN=root \
  try root_cert root

CA_ID=root CN=l1_leaf \
  try issue_cert l1_leaf leaf_cert_san_dns as_der

CA_ID=root CN=l1_interm \
  try issue_cert l1_interm ca_cert as_der

CA_ID=l1_interm CN=l2_leaf \
  try issue_cert l2_leaf leaf_cert_san_dns as_der

try rm -rf out
