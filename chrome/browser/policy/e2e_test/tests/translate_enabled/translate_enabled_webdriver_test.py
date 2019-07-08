# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time
import test_util
from absl import app, flags
from selenium import webdriver
from selenium.webdriver.chrome.options import Options

# A URL that is in a different language than our Chrome language. It should be
# a site stable enough for us to rely on for our tests, and have a somewhat
# static string we can use to tell if translate worked (see below).
URL = "https://zh.wikipedia.org/wiki/Chromium"

# The magic string to look for on the translated page.
# This string must NOT be on the original page.
MAGIC_STRING_ENGLISH = "Wikipedia, the free encyclopedia"

# We automatically accept Translate prompts from Chinese (Simpl.) to English.
# Ideally we would detect the Translate prompt and return TRUE/FALSE based on
# that, but webdriver doesn't support this AFAIK.
PREFS = {"translate_whitelists": {"zh-CN": "en"}}

FLAGS = flags.FLAGS

flags.DEFINE_bool('incognito', False,
                  'Set flag to open Chrome in incognito mode.')


def main(argv):
  driver = test_util.create_chrome_webdriver(
      incognito=FLAGS.incognito, prefs=PREFS)
  driver.get(URL)

  time.sleep(10)

  output = driver.find_element_by_css_selector('html').text.encode('utf-8')

  if MAGIC_STRING_ENGLISH in output:
    print "TRUE"
  else:
    print "FALSE"

  driver.quit()


if __name__ == '__main__':
  app.run(main)
