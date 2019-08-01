# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from selenium import webdriver
import os
from absl import app, flags

FLAGS = flags.FLAGS
flags.DEFINE_string('user_data_dir', None, 'Need specify user data dir to test')
flags.mark_flag_as_required('user_data_dir')


def main(argv):
  policy_url = "chrome://policy"
  version_url = "chrome://version"

  # Verify the user data dir is not exsiting before launch the Chrome
  print "User data before running chrome is " + str(
      os.path.isdir(FLAGS.user_data_dir))

  # Launch real Chrome
  os.system('start chrome --remote-debugging-port=9222')

  options = webdriver.ChromeOptions()
  # Add option for connecting chromedriver with Chrome
  options.add_experimental_option("debuggerAddress", "localhost:9222")
  os.environ["CHROME_LOG_FILE"] = r"c:\temp\chrome_log.txt"

  driver = webdriver.Chrome(
      "C:/ProgramData/chocolatey/lib/chromedriver/tools/chromedriver.exe",
      chrome_options=options,
      service_args=["--verbose", r"--log-path=c:\temp\chromedriver.log"])

  try:
    # Verify User Data Dir in chrome://policy page
    driver.get(policy_url)
    print driver.find_element_by_css_selector('html').text.encode('utf-8')

    # Verfiy User Data Dir used in chrome://version
    driver.get(version_url)
    print "Profile path is " + driver.find_element_by_id("profile_path").text

    # Verify if UserDataDir folder is created
    print "User data dir creation is " + str(os.path.isdir(FLAGS.user_data_dir))
  except Exception as error:
    print error
  finally:
    driver.quit()
    os.system('taskkill /f /im chrome.exe')


if __name__ == '__main__':
  app.run(main)
