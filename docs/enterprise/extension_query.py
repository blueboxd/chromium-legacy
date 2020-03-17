#!/usr/bin/env python
# Copyright (c) 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Transform CBCM Takeout API Data (Python3)."""

import argparse
import csv
import json
import sys

import google_auth_httplib2

from httplib2 import Http
from google.oauth2.service_account import Credentials


def ComputeExtensionsList(extensions_list, data):
  """Computes list of machines that have an extension.

  This sample function processes the |data| retrieved from the Takeout API and
  calculates the list of machines that have installed each extension listed in
  the data.

  Args:
    extensions_list: the extension list dictionary to fill.
    data: the data fetched from the Takeout API.
  """
  for device in data['browsers']:
    if 'browsers' not in device:
      continue
    for browser in device['browsers']:
      if 'profiles' not in browser:
        continue
      for profile in browser['profiles']:
        if 'extensions' not in profile:
          continue
        for extension in profile['extensions']:
          key = extension['extensionId']
          if 'version' in extension:
            key = key + ' @ ' + extension['version']
          if key not in extensions_list:
            current_extension = {
                'name':
                    extension['name'],
                'permissions':
                    extension['permissions']
                    if 'permissions' in extension else '',
                'installed':
                    set(),
                'disabled':
                    set(),
                'forced':
                    set()
            }
          else:
            current_extension = extensions_list[key]

          machine_name = device['machineName']
          current_extension['installed'].add(machine_name)
          if 'installType' in extension and extension['installType'] == 3:
            current_extension['forced'].add(machine_name)
          if 'disabled' in extension and extension['disabled']:
            current_extension['disabled'].add(machine_name)

          extensions_list[key] = current_extension


def DictToList(data, key_name='id'):
  """Converts a dict into a list.

  The value of each member of |data| must also be a dict. The original key for
  the value will be inlined into the value, under the |key_name| key.

  Args:
    data: a dict where every value is a dict
    key_name: the name given to the key that is inlined into the dict's values

  Yields:
    The values from |data|, with each value's key inlined into the value.
  """
  assert isinstance(data, dict), '|data| must be a dict'
  for key, value in data.items():
    assert isinstance(value, dict), '|value| must contain dict items'
    value[key_name] = key
    yield value


def Flatten(data):
  """Flattens lists inside |data|, one level deep.

  This function will flatten each dictionary key in |data| into a single row
  so that it can be written to a CSV file.

  Args:
    data: the data to be flattened.

  Yields:
    A list of dict objects whose lists or sets have been flattened.
  """
  for item in data:
    counts = {}
    for prop, value in item.items():
      if isinstance(value, (list, set)):
        item[prop] = ', '.join(sorted(value))
        counts['num_' + prop] = len(value)

      assert isinstance(item[prop],
                        (int, bool, str)), ('unexpected type for item: %s' %
                                            type(item[prop]).__name__)

    item.update(counts)
    yield item


def ExtensionListAsCsv(extensions_list, csv_filename, sort_column='name'):
  """Saves an extensions list to a CSV file.

  Args:
    extensions_list: an extensions list as returned by ComputeExtensionsList
    csv_filename: the name of the CSV file to save
    sort_column: the name of the column by which to sort the data
  """
  flattened_list = list(Flatten(DictToList(extensions_list)))
  fieldnames = flattened_list[0].keys() if flattened_list else []

  desired_column_order = [
      'id', 'name', 'num_permissions', 'num_installed', 'num_disabled',
      'num_forced', 'permissions', 'installed', 'disabled', 'forced'
  ]

  # Order the columns as desired. Columns other than those in
  # |desired_column_order| will be in an unspecified order after these columns.
  ordered_fieldnames = list(c for c in desired_column_order if c in fieldnames)

  ordered_fieldnames.extend(
      [x for x in desired_column_order if x not in ordered_fieldnames])
  with open(csv_filename, mode='w', encoding='utf-8') as csv_file:
    writer = csv.DictWriter(csv_file, fieldnames=ordered_fieldnames)
    writer.writeheader()
    for row in sorted(flattened_list, key=lambda ext: ext[sort_column]):
      writer.writerow(row)


def main(args):
  if not args.admin_email:
    print('admin_email must be specified.')
    sys.exit(1)

  if not args.service_account_key_path:
    print('service_account_key_path must be specified.')
    sys.exit(1)

  # Load the json format key that you downloaded from the Google API
  # Console when you created your service account. For p12 keys, use the
  # from_p12_keyfile method of ServiceAccountCredentials and specify the
  # service account email address, p12 keyfile, and scopes.
  service_credentials = Credentials.from_service_account_file(
      args.service_account_key_path,
      scopes=[
          'https://www.googleapis.com/auth/admin.directory.device.chromebrowsers.readonly'
      ],
      subject=args.admin_email)

  try:
    http = google_auth_httplib2.AuthorizedHttp(service_credentials, http=Http())
    extensions_list = {}
    base_request_url = 'https://admin.googleapis.com/admin/directory/v1.1beta1/customer/my_customer/devices/chromebrowsers'
    request_parameters = ''
    browsers_processed = 0
    while True:
      print('Making request to server ...')
      response = http.request(base_request_url + '?' + request_parameters,
                              'GET')[1]
      if isinstance(response, bytes):
        response = response.decode('utf-8')
      data = json.loads(response)

      browsers_in_data = len(data['browsers'])
      print('Request returned %s results, analyzing ...' % (browsers_in_data))
      ComputeExtensionsList(extensions_list, data)
      browsers_processed += browsers_in_data

      if 'nextPageToken' not in data or not data['nextPageToken']:
        break

      print('%s browsers processed.' % (browsers_processed))

      if (args.max_browsers_to_process is not None and
          args.max_browsers_to_process <= browsers_processed):
        print('Stopping at %s browsers processed.' % (browsers_processed))
        break

      request_parameters = ('pageToken={}').format(data['nextPageToken'])
  finally:
    print('Analyze results ...')
    ExtensionListAsCsv(extensions_list, args.extension_list_csv)
    print("Results written to '%s'" % (args.extension_list_csv))


if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='CBCM Extension Analyzer')
  parser.add_argument(
      '-k',
      '--service_account_key_path',
      metavar='FILENAME',
      required=True,
      help='The service account key file used to make API requests.')
  parser.add_argument(
      '-a',
      '--admin_email',
      required=True,
      help='The admin user used to make the API requests.')
  parser.add_argument(
      '-x',
      '--extension_list_csv',
      metavar='FILENAME',
      default='./extension_list.csv',
      help='Generate an extension list to the specified CSV '
      'file')
  parser.add_argument(
      '-m',
      '--max_browsers_to_process',
      type=int,
      help='Maximum number of browsers to process. (Must be > 0).')
  args = parser.parse_args()

  if (args.max_browsers_to_process is not None and
      args.max_browsers_to_process <= 0):
    print('max_browsers_to_process must be > 0.')
    parser.print_help()
    sys.exit(1)

  main(args)
