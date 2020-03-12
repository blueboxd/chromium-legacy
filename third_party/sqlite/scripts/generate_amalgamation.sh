#!/bin/sh
#
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates two amalgamated sources for SQLite.
# One is for Chromium, it has some features disabled. The other one is for
# developer tools, with a fuller set of features.
# The build flag sets should be in sync with the defines in BUILD.gn,
# "chromium_sqlite3_compile_options" and "common_sqlite3_compile_options"
# configs.

set -o errexit  # Stop the script on the first error.
set -o nounset  # Catch un-initialized variables.

cd patched

BUILD_GN_FLAGS_COMMON="\
    -DSQLITE_DISABLE_FTS3_UNICODE \
    -DSQLITE_DISABLE_FTS4_DEFERRED \
    -DSQLITE_ENABLE_FTS3 \
    -DSQLITE_ENABLE_ICU \
    -DSQLITE_DEFAULT_FILE_PERMISSIONS=0600 \
    -DSQLITE_DEFAULT_LOOKASIDE=0,0 \
    -DSQLITE_DEFAULT_MEMSTATUS=1 \
    -DSQLITE_DEFAULT_PAGE_SIZE=4096 \
    -DSQLITE_DEFAULT_PCACHE_INITSZ=0 \
    -DSQLITE_LIKE_DOESNT_MATCH_BLOBS \
    -DSQLITE_HAVE_ISNAN \
    -DSQLITE_MAX_WORKER_THREADS=0 \
    -DSQLITE_MAX_MMAP_SIZE=268435456 \
    -DSQLITE_OMIT_DECLTYPE \
    -DSQLITE_OMIT_DEPRECATED \
    -DSQLITE_OMIT_LOAD_EXTENSION \
    -DSQLITE_OMIT_PROGRESS_CALLBACK \
    -DSQLITE_OMIT_SHARED_CACHE \
    -DSQLITE_SECURE_DELETE \
    -DSQLITE_THREADSAFE=1 \
    -DSQLITE_USE_ALLOCA \
"

BUILD_GN_FLAGS_CHROMIUM="\
    -DSQLITE_OMIT_ANALYZE \
    -DSQLITE_OMIT_AUTOINIT \
    -DSQLITE_OMIT_AUTORESET \
    -DSQLITE_OMIT_COMPILEOPTION_DIAGS \
    -DSQLITE_OMIT_COMPLETE \
    -DSQLITE_OMIT_EXPLAIN \
    -DSQLITE_OMIT_GET_TABLE \
    -DSQLITE_OMIT_LOOKASIDE \
    -DSQLITE_OMIT_TCL_VARIABLE \
    -DSQLITE_OMIT_REINDEX \
    -DSQLITE_OMIT_TRACE \
    -DSQLITE_OMIT_UPSERT \
    -DSQLITE_OMIT_WINDOWFUNC \
"

BUILD_GN_FLAGS_DEV="\
    -DSQLITE_ENABLE_EXPLAIN_COMMENTS \
"

generate_amalgamation() {
  local build_flags=$1
  local amalgamation_path=$2

  mkdir build
  cd build

  ../configure \
      CFLAGS="-Os $build_flags $(icu-config --cppflags)" \
      LDFLAGS="$(icu-config --ldflags)" \
      --disable-load-extension \
      --enable-amalgamation \
      --enable-threadsafe

  make shell.c sqlite3.h sqlite3.c
  cp -f sqlite3.h sqlite3.c ../$amalgamation_path/

  # shell.c must be placed in a different directory from sqlite3.h, because it
  # contains an '#include "sqlite3.h"' that we want to resolve to our custom
  # //third_party/sqlite/sqlite3.h, not to the sqlite3.h produced here.
  mkdir -p ../$amalgamation_path/shell/
  cp -f shell.c ../$amalgamation_path/shell/

  cd ..
  rm -rf build

  ../scripts/extract_sqlite_api.py $amalgamation_path/sqlite3.h \
                                   $amalgamation_path/rename_exports.h
}

generate_amalgamation "$BUILD_GN_FLAGS_COMMON $BUILD_GN_FLAGS_CHROMIUM" ../amalgamation
generate_amalgamation "$BUILD_GN_FLAGS_COMMON $BUILD_GN_FLAGS_DEV" ../amalgamation_dev

