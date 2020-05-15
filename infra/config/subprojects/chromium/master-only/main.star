# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

exec('./ci.star')
exec('./gpu.try.star')
exec('./swangle.try.star')
exec('./try.star')

exec('./consoles/android.packager.star')
exec('./consoles/luci.chromium.try.star')
exec('./consoles/sheriff.ios.star')

exec('./fallback-cq.star')
