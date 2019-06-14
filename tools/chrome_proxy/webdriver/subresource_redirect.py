# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from common import TestDriver
from common import IntegrationTest
from decorators import ChromeVersionEqualOrAfterM

LITEPAGES_REGEXP = r'https://\w+\.litepages\.googlezip\.net/.*'

class SubresourceRedirect(IntegrationTest):

  @ChromeVersionEqualOrAfterM(77)
  def testCompressImage(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-subresource-redirect')
      test_driver.LoadURL('https://check.googlezip.net/static/index.html')

      image_responses = 0

      for response in test_driver.GetHTTPResponses():
        content_type = ''
        if 'content-type' in response.response_headers:
          content_type = response.response_headers['content-type']
        if ('image/' in content_type
          and re.match(LITEPAGES_REGEXP, response.url)
          and 200 == response.status):
          image_responses += 1

      self.assertEqual(5, image_responses)

  # TODO(harrisonsean): Add compression server bypass test

  @ChromeVersionEqualOrAfterM(77)
  def testNoCompressNonImage(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-subresource-redirect')
      test_driver.LoadURL('https://check.googlezip.net/testvideo.html')

      image_responses = 0

      for response in test_driver.GetHTTPResponses():
        content_type = ''
        if 'content-type' in response.response_headers:
          content_type = response.response_headers['content-type']
        if ('image/' in content_type
          and re.match(LITEPAGES_REGEXP, response.url)
          and 200 == response.status):
          image_responses += 1

      self.assertEqual(0, image_responses)

  @ChromeVersionEqualOrAfterM(77)
  def testNoCompressNonHTTPS(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-subresource-redirect')
      test_driver.LoadURL('http://check.googlezip.net/static/index.html')

      image_responses = 0

      for response in test_driver.GetHTTPResponses():
        content_type = ''
        if 'content-type' in response.response_headers:
          content_type = response.response_headers['content-type']
        if ('image/' in content_type
          and re.match(LITEPAGES_REGEXP, response.url)
          and 200 == response.status):
          image_responses += 1

      self.assertEqual(0, image_responses)

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
