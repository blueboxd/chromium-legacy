# Android WebView CTS Test Configuration

Test apk(s) and tests to run on Android are configurable on a per
Android release basis by editing the webview_cts_gcs_path.json file in this
directory.

## File format
```json
{
  {
    "<Android dessert letter>": {
      "arch": {
        "<arch1>": {
          "filename": "<relative path to cts_archive_dir of cts zip>",
          "_origin": "<CTS zip download url>",
          "unzip_dir": "<relative path to work directory where cts should be unzipped to>"
        },
        "<arch2>": {
          "filename": "<relative path to cts_archive_dir of cts zip>",
          "_origin": "<CTS zip download url>",
          "unzip_dir": "<relative path to work directory where cts should be unzipped to>"
        }
      },
      "test_runs": [
        {
          "apk": "location of the test apk in the cts zip file",
          "excludes": [
            {
              "match": "<class#testcase (wildcard supported) expression of test to skip>",
              "_bug_id": "<bug reference comment, optional>"
            }
          ]
        },
        {
          "apk": "location of the test apk in the cts zip file",
          "includes": [
            {
              "match": "<class#testcase (wildcard supported) expression of test to run>"
            }
          ]
        }
      ]
    }
  },
  ...
}
```

*** note
**Note:** Test names in the include/exclude list could change between releases,
please adjust them accordingly.
***

## Skipping tests
Add entries to the "exlcudes" list for the respective apk under "test_runs".

*** note
**Note:** If includes nor excludes are specified, all tests in the apk will run.
***
