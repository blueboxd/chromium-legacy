# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load('//project.star', 'master_only_exec')

exec('./ci.star')
exec('./try.star')

master_only_exec('./master-only/main.star')
