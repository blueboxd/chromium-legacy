# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from force_google_safe_search.force_google_safe_search import *
from homepage.homepage import *
# TODO(feiling): Fix RestoreOnStartupTest on LUCI bots.
# from restore_on_startup.restore_on_startup import *
from popups_allowed.popups_allowed import *
from url_blacklist.url_blacklist import *
from url_whitelist.url_whitelist import *
from extension_blacklist.extension_blacklist import *
from extension_whitelist.extension_whitelist import *
# TODO(mbinette): Fix TranslateEnabledTest on LUCI bots.
# from translate_enabled.translate_enabled import *
from youtube_restrict.youtube_restrict import *
