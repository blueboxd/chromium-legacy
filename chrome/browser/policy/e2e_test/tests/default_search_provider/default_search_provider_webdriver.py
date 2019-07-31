# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from selenium import webdriver
from pywinauto.application import Application
import os

# Set up ChromeDriver
options = webdriver.ChromeOptions()
options.add_argument("--force-renderer-accessibility")

os.environ["CHROME_LOG_FILE"] = r"c:\temp\chrome_log.txt"

driver = webdriver.Chrome(
    "C:/ProgramData/chocolatey/lib/chromedriver/tools/chromedriver.exe",
    chrome_options=options,
    service_args=["--verbose", r"--log-path=c:\temp\chromedriver.log"])

try:
  app = Application(backend="uia")
  app.connect(title_re='.*Chrome|.*Chromium')
  omnibox = app.top_window() \
            .child_window(title="Address and search bar", control_type="Edit")
  omnibox.set_edit_text('anything').type_keys('{ENTER}')
  print driver.current_url
except Exception as error:
  print error
finally:
  driver.quit()
