# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from selenium import webdriver
import os
from pywinauto.application import Application

driver = webdriver.Chrome(
    "C:/ProgramData/chocolatey/lib/chromedriver/tools/chromedriver.exe")

try:
  app = Application(backend="uia")
  app.connect(title_re='.*Chrome|.*Chromium')

  # Use shortcut Alt+HOME to go to the home page
  app.top_window().type_keys("%{HOME}")

  print 'homepage:%s' % driver.current_url
finally:
  driver.quit()
