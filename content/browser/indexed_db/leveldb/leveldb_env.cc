// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/leveldb/leveldb_env.h"

namespace content {

LevelDBEnv::LevelDBEnv() : ChromiumEnv("LevelDBEnv.IDB") {}

LevelDBEnv* LevelDBEnv::Get() {
  static base::NoDestructor<LevelDBEnv> g_leveldb_env;
  return g_leveldb_env.get();
}

}  // namespace content
