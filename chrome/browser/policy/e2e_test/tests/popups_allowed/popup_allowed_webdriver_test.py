# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from selenium import webdriver

testSite = "http://www.popuptest.com/popuptest1.html"
options = webdriver.ChromeOptions()
options.add_experimental_option('excludeSwitches', ['disable-popup-blocking'])
exe_path = "C:/ProgramData/chocolatey/lib/chromedriver/tools/chromedriver.exe"
driver = webdriver.Chrome(
    exe_path,
    chrome_options=options,
    service_args=["--verbose", r"--log-path=c:\temp\chromedriver.log"])
driver.implicitly_wait(5)
driver.get(testSite)
handles = driver.window_handles
print len(handles)
driver.quit()
