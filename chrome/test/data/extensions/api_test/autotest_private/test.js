// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var defaultTests = [
  // logout/restart/shutdown don't do anything as we don't want to kill the
  // browser with these tests.
  function logout() {
    chrome.autotestPrivate.logout();
    chrome.test.succeed();
  },
  function restart() {
    chrome.autotestPrivate.restart();
    chrome.test.succeed();
  },
  function shutdown() {
    chrome.autotestPrivate.shutdown(true);
    chrome.test.succeed();
  },
  function lockScreen() {
    chrome.autotestPrivate.lockScreen();
    chrome.test.succeed();
  },
  function simulateAsanMemoryBug() {
    chrome.autotestPrivate.simulateAsanMemoryBug();
    chrome.test.succeed();
  },
  function loginStatus() {
    chrome.autotestPrivate.loginStatus(
        chrome.test.callbackPass(function(status) {
          chrome.test.assertEq(typeof status, 'object');
          chrome.test.assertTrue(status.hasOwnProperty("isLoggedIn"));
          chrome.test.assertTrue(status.hasOwnProperty("isOwner"));
          chrome.test.assertTrue(status.hasOwnProperty("isScreenLocked"));
          chrome.test.assertTrue(status.hasOwnProperty("isRegularUser"));
          chrome.test.assertTrue(status.hasOwnProperty("isGuest"));
          chrome.test.assertTrue(status.hasOwnProperty("isKiosk"));
          chrome.test.assertTrue(status.hasOwnProperty("email"));
          chrome.test.assertTrue(status.hasOwnProperty("displayEmail"));
          chrome.test.assertTrue(status.hasOwnProperty("userImage"));
        }));
  },
  function getExtensionsInfo() {
    chrome.autotestPrivate.getExtensionsInfo(
        chrome.test.callbackPass(function(extInfo) {
          chrome.test.assertEq(typeof extInfo, 'object');
          chrome.test.assertTrue(extInfo.hasOwnProperty('extensions'));
          chrome.test.assertTrue(extInfo.extensions.constructor === Array);
          for (var i = 0; i < extInfo.extensions.length; ++i) {
            var extension = extInfo.extensions[i];
            chrome.test.assertTrue(extension.hasOwnProperty('id'));
            chrome.test.assertTrue(extension.hasOwnProperty('version'));
            chrome.test.assertTrue(extension.hasOwnProperty('name'));
            chrome.test.assertTrue(extension.hasOwnProperty('publicKey'));
            chrome.test.assertTrue(extension.hasOwnProperty('description'));
            chrome.test.assertTrue(extension.hasOwnProperty('backgroundUrl'));
            chrome.test.assertTrue(extension.hasOwnProperty(
                'hostPermissions'));
            chrome.test.assertTrue(
                extension.hostPermissions.constructor === Array);
            chrome.test.assertTrue(extension.hasOwnProperty(
                'effectiveHostPermissions'));
            chrome.test.assertTrue(
                extension.effectiveHostPermissions.constructor === Array);
            chrome.test.assertTrue(extension.hasOwnProperty(
                'apiPermissions'));
            chrome.test.assertTrue(
                extension.apiPermissions.constructor === Array);
            chrome.test.assertTrue(extension.hasOwnProperty('isComponent'));
            chrome.test.assertTrue(extension.hasOwnProperty('isInternal'));
            chrome.test.assertTrue(extension.hasOwnProperty(
                'isUserInstalled'));
            chrome.test.assertTrue(extension.hasOwnProperty('isEnabled'));
            chrome.test.assertTrue(extension.hasOwnProperty(
                'allowedInIncognito'));
            chrome.test.assertTrue(extension.hasOwnProperty('hasPageAction'));
          }
        }));
  },
  function setTouchpadSensitivity() {
    chrome.autotestPrivate.setTouchpadSensitivity(3);
    chrome.test.succeed();
  },
  function setTapToClick() {
    chrome.autotestPrivate.setTapToClick(true);
    chrome.test.succeed();
  },
  function setThreeFingerClick() {
    chrome.autotestPrivate.setThreeFingerClick(true);
    chrome.test.succeed();
  },
  function setTapDragging() {
    chrome.autotestPrivate.setTapDragging(false);
    chrome.test.succeed();
  },
  function setNaturalScroll() {
    chrome.autotestPrivate.setNaturalScroll(true);
    chrome.test.succeed();
  },
  function setMouseSensitivity() {
    chrome.autotestPrivate.setMouseSensitivity(3);
    chrome.test.succeed();
  },
  function setPrimaryButtonRight() {
    chrome.autotestPrivate.setPrimaryButtonRight(false);
    chrome.test.succeed();
  },
  function setMouseReverseScroll() {
    chrome.autotestPrivate.setMouseReverseScroll(true);
    chrome.test.succeed();
  },
  function getVisibleNotifications() {
    chrome.autotestPrivate.getVisibleNotifications(function(){});
    chrome.test.succeed();
  },
  // In this test, ARC is available but not managed and not enabled by default.
  function getPlayStoreState() {
    chrome.autotestPrivate.getPlayStoreState(function(state) {
      chrome.test.assertTrue(state.allowed);
      chrome.test.assertFalse(state.enabled);
      chrome.test.assertFalse(state.managed);
      chrome.test.succeed();
    });
  },
  // This test turns ARC enabled state to ON.
  function setPlayStoreEnabled() {
    chrome.autotestPrivate.setPlayStoreEnabled(true, function() {
      chrome.test.assertNoLastError();
      chrome.autotestPrivate.getPlayStoreState(function(state) {
        chrome.test.assertTrue(state.allowed);
        chrome.test.assertTrue(state.enabled);
        chrome.test.assertFalse(state.managed);
        chrome.test.succeed();
      });
    });
  },
  function getHistogramExists() {
    // Request an arbitrary histogram that is reported once at startup and seems
    // unlikely to go away.
    chrome.autotestPrivate.getHistogram(
        "Startup.BrowserProcessImpl_PreMainMessageLoopRunTime",
        chrome.test.callbackPass(function(histogram) {
          chrome.test.assertEq(typeof histogram, 'object');
          chrome.test.assertEq(histogram.buckets.length, 1);
          chrome.test.assertEq(histogram.buckets[0].count, 1);
          chrome.test.assertTrue(
              histogram.buckets[0].max > histogram.buckets[0].min);
        }));
  },
  function getHistogramMissing() {
    chrome.autotestPrivate.getHistogram(
        'Foo.Nonexistent',
        chrome.test.callbackFail('Histogram Foo.Nonexistent not found'));
  },
  // This test verifies that Play Store window is not shown by default but
  // Chrome is shown.
  function isAppShown() {
    chrome.autotestPrivate.isAppShown('cnbgggchhmkkdmeppjobngjoejnihlei',
        function(appShown) {
          chrome.test.assertFalse(appShown);
          chrome.test.assertNoLastError();

          // Chrome is running.
          chrome.autotestPrivate.isAppShown('mgndgikekgjfcpckkfioiadnlibdjbkf',
              function(appShown) {
                 chrome.test.assertTrue(appShown);
                 chrome.test.assertNoLastError();
                 chrome.test.succeed();
            });
        });
  },
  // This launches and closes Chrome.
  function launchCloseApp() {
    chrome.autotestPrivate.launchApp('mgndgikekgjfcpckkfioiadnlibdjbkf',
        function() {
          chrome.test.assertNoLastError();
          chrome.autotestPrivate.isAppShown('mgndgikekgjfcpckkfioiadnlibdjbkf',
              function(appShown) {
                chrome.test.assertNoLastError();
                chrome.test.assertTrue(appShown);
                chrome.test.succeed();
               });
        });
  },
  function setCrostiniEnabled() {
    chrome.autotestPrivate.setCrostiniEnabled(true, chrome.test.callbackFail(
        'Crostini is not available for the current user'));
  },
  function runCrostiniInstaller() {
    chrome.autotestPrivate.runCrostiniInstaller(chrome.test.callbackFail(
        'Crostini is not available for the current user'));
  },
  // This sets a Crostini app's "scaled" property in the app registry.
  // When the property is set to true, the app will be launched in low display
  // density.
  function setCrostiniAppScaled() {
    chrome.autotestPrivate.setCrostiniAppScaled(
        'nodabfiipdopnjihbfpiengllkohmfkl', true,
        function() {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },
  function bootstrapMachineLearningService() {
    chrome.autotestPrivate.bootstrapMachineLearningService(
        chrome.test.callbackFail('ML Service connection error'));
  },
  function runCrostiniUninstaller() {
    chrome.autotestPrivate.runCrostiniUninstaller(chrome.test.callbackFail(
        'Crostini is not available for the current user'));
  },
  function exportCrostini() {
    chrome.autotestPrivate.exportCrostini('backup', chrome.test.callbackFail(
        'Crostini is not available for the current user'));
  },
  function importCrostini() {
    chrome.autotestPrivate.importCrostini('backup', chrome.test.callbackFail(
        'Crostini is not available for the current user'));
  },
  function takeScreenshot() {
    chrome.autotestPrivate.takeScreenshot(
      function(base64Png) {
        chrome.test.assertTrue(base64Png.length > 0);
        chrome.test.assertNoLastError();
        chrome.test.succeed();
      }
    )
  },
  function getPrinterList() {
    chrome.autotestPrivate.getPrinterList(function(){
      chrome.test.succeed();
    });
  },
  function setAssistantEnabled() {
    chrome.autotestPrivate.setAssistantEnabled(true, 1000 /* timeout_ms */,
        chrome.test.callbackFail(
            'Assistant not allowed - state: 9'));
  },
  function sendAssistantTextQuery() {
    chrome.autotestPrivate.sendAssistantTextQuery(
        'what time is it?' /* query */,
        1000 /* timeout_ms */,
        chrome.test.callbackFail(
            'Assistant not allowed - state: 9'));
  },
  function setWhitelistedPref() {
    chrome.autotestPrivate.setWhitelistedPref(
        'settings.voice_interaction.hotword.enabled' /* pref_name */,
        true /* value */,
        chrome.test.callbackFail(
            'Assistant not allowed - state: 9'));
  },
  // This test verifies that getArcState returns provisioned False in case ARC
  // is not provisioned by default.
  function arcNotProvisioned() {chrome.autotestPrivate.getArcState(
    function(state) {
      chrome.test.assertFalse(state.provisioned);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  // This test verifies that ARC Terms of Service are needed by default.
  function arcTosNeeded() {
    chrome.autotestPrivate.getArcState(function(state) {
      chrome.test.assertTrue(state.tosNeeded);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  // No any ARC app by default
  function getArcApp() {
    chrome.autotestPrivate.getArcApp(
         'bifanmfigailifmdhaomnmchcgflbbdn',
         chrome.test.callbackFail('App is not available'));
  },
  // No any ARC package by default
  function getArcPackage() {
    chrome.autotestPrivate.getArcPackage(
         'fake.package',
         chrome.test.callbackFail('Package is not available'));
  },
  // Launch fails, no any ARC app by default
  function launchArcApp() {
    chrome.autotestPrivate.launchArcApp(
        'bifanmfigailifmdhaomnmchcgflbbdn',
        '#Intent;',
        function(appLaunched) {
          chrome.test.assertFalse(appLaunched);
          chrome.test.succeed();
        });
  },
  // This gets the primary display's scale factor.
  function getPrimaryDisplayScaleFactor() {
    chrome.autotestPrivate.getPrimaryDisplayScaleFactor(
        function(scaleFactor) {
          chrome.test.assertNoLastError();
          chrome.test.assertTrue(scaleFactor >= 1.0);
          chrome.test.succeed();
        });
  },
  // Check if tablet mode is enabled.
  function isTabletModeEnabled() {
    chrome.autotestPrivate.isTabletModeEnabled(
        function(enabled) {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },
  // This test verifies that entering tablet mode works as expected.
  function setTabletModeEnabled() {
    chrome.autotestPrivate.setTabletModeEnabled(true, function(isEnabled){
      chrome.test.assertTrue(isEnabled);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  // This test verifies that leaving tablet mode works as expected.
  function setTabletModeDisabled() {
    chrome.autotestPrivate.setTabletModeEnabled(false, function(isEnabled){
      chrome.test.assertFalse(isEnabled);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  // This test verifies that changing the shelf behavior works as expected.
  function setShelfAutoHideBehavior() {
    // Using shelf from primary display.
    var displayId = "-1";
    chrome.system.display.getInfo(function(info) {
      var l = info.length;
      for (var i = 0; i < l; i++) {
        if (info[i].isPrimary === true) {
          displayId = info[i].id;
          break;
        }
      }
      chrome.test.assertTrue(displayId != "-1");
      // SHELF_AUTO_HIDE_ALWAYS_HIDDEN not supported by shelf_prefs.
      // TODO(ricardoq): Use enums in IDL instead of hardcoded strings.
      var behaviors = ["always", "never"];
      var l = behaviors.length;
      for (var i = 0; i < l; i++) {
        var behavior = behaviors[i];
        chrome.autotestPrivate.setShelfAutoHideBehavior(displayId, behavior,
            function() {
          chrome.test.assertNoLastError();
          chrome.autotestPrivate.getShelfAutoHideBehavior(displayId,
              function(newBehavior) {
            chrome.test.assertNoLastError();
            chrome.test.assertEq(behavior, newBehavior);
          });
        });
      }
      chrome.test.succeed();
    });
  },
  // This test verifies that changing the shelf alignment works as expected.
  function setShelfAlignment() {
    // Using shelf from primary display.
    var displayId = "-1";
    chrome.system.display.getInfo(function(info) {
      var l = info.length;
      for (var i = 0; i < l; i++) {
        if (info[i].isPrimary === true) {
          displayId = info[i].id;
          break;
        }
      }
      chrome.test.assertTrue(displayId != "-1");
      // SHELF_ALIGNMENT_BOTTOM_LOCKED not supported by shelf_prefs.
      var alignments = [chrome.autotestPrivate.ShelfAlignmentType.LEFT,
        chrome.autotestPrivate.ShelfAlignmentType.BOTTOM,
        chrome.autotestPrivate.ShelfAlignmentType.RIGHT]
      var l = alignments.length;
      for (var i = 0; i < l; i++) {
        var alignment = alignments[i];
        chrome.autotestPrivate.setShelfAlignment(displayId, alignment,
            function() {
          chrome.test.assertNoLastError();
          chrome.autotestPrivate.getShelfAlignment(displayId,
              function(newAlignment) {
            chrome.test.assertNoLastError();
            chrome.test.assertEq(newAlignment, alignment);
          });
        });
      }
      chrome.test.succeed();
    });
  },
];

var arcEnabledTests = [
  // This test verifies that getArcState returns provisioned True in case ARC
  // provisioning is done.
  function arcProvisioned() {chrome.autotestPrivate.getArcState(
    function(state) {
      chrome.test.assertTrue(state.provisioned);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  // This test verifies that ARC Terms of Service are not needed in case ARC is
  // provisioned and Terms of Service are accepted.
  function arcTosNotNeeded() {
    chrome.autotestPrivate.getArcState(function(state) {
      chrome.test.assertFalse(state.tosNeeded);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  // ARC app is available
  function getArcApp() {
    // bifanmfigailifmdhaomnmchcgflbbdn id is taken from
    //   ArcAppListPrefs::GetAppId(
    //   "fake.package", "fake.package.activity");
    chrome.autotestPrivate.getArcApp('bifanmfigailifmdhaomnmchcgflbbdn',
        chrome.test.callbackPass(function(appInfo) {
          chrome.test.assertNoLastError();
          // See AutotestPrivateArcEnabled for constants.
          chrome.test.assertEq('Fake App', appInfo.name);
          chrome.test.assertEq('fake.package', appInfo.packageName);
          chrome.test.assertEq('fake.package.activity', appInfo.activity);
          chrome.test.assertEq('', appInfo.intentUri);
          chrome.test.assertEq('', appInfo.iconResourceId);
          chrome.test.assertEq(0, appInfo.lastLaunchTime);
          // Install time is set right before this call. Assume we are 5
          // min maximum after setting the install time.
          chrome.test.assertTrue(Date.now() >= appInfo.installTime);
          chrome.test.assertTrue(
              Date.now() <= appInfo.installTime + 5 * 60 * 1000.0);
          chrome.test.assertEq(false, appInfo.sticky);
          chrome.test.assertEq(false, appInfo.notificationsEnabled);
          chrome.test.assertEq(true, appInfo.ready);
          chrome.test.assertEq(false, appInfo.suspended);
          chrome.test.assertEq(true, appInfo.showInLauncher);
          chrome.test.assertEq(false, appInfo.shortcut);
          chrome.test.assertEq(true, appInfo.launchable);

          chrome.test.succeed();
        }));
  },
  // ARC is available but app does not exist
  function getArcNonExistingApp() {
    chrome.autotestPrivate.getArcApp(
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
        chrome.test.callbackFail('App is not available'));
  },
  // ARC package is available
  function getArcPackage() {
    chrome.autotestPrivate.getArcPackage('fake.package',
        chrome.test.callbackPass(function(packageInfo) {
          chrome.test.assertNoLastError();
          // See AutotestPrivateArcEnabled for constants.
          chrome.test.assertEq('fake.package', packageInfo.packageName);
          chrome.test.assertEq(10, packageInfo.packageVersion);
          chrome.test.assertEq('100', packageInfo.lastBackupAndroidId);
          // Backup time is set right before this call. Assume we are 5
          // min maximum after setting the backup time.
          chrome.test.assertTrue(Date.now() >= packageInfo.lastBackupTime);
          chrome.test.assertTrue(
              Date.now() <= packageInfo.lastBackupTime + 5 * 60 * 1000.0);
          chrome.test.assertEq(true, packageInfo.shouldSync);
          chrome.test.assertEq(false, packageInfo.system);
          chrome.test.assertEq(false, packageInfo.vpnProvider);
          chrome.test.succeed();
        }));
  },
  // Launch existing ARC app
  function launchArcApp() {
    chrome.autotestPrivate.launchArcApp(
        'bifanmfigailifmdhaomnmchcgflbbdn',
        '#Intent;',
        function(appLaunched) {
          chrome.test.assertNoLastError();
          chrome.test.assertTrue(appLaunched);
          chrome.test.succeed();
        });
  },
  // Launch non-existing ARC app
  function launchNonExistingApp() {
    chrome.autotestPrivate.launchArcApp(
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
        '#Intent;',
        function(appLaunched) {
          chrome.test.assertNoLastError();
          chrome.test.assertFalse(appLaunched);
          chrome.test.succeed();
        });
  },
];

var policyTests = [
  function getAllEnterpricePolicies() {
    chrome.autotestPrivate.getAllEnterprisePolicies(
      chrome.test.callbackPass(function(policydata) {
        chrome.test.assertNoLastError();
        // See AutotestPrivateWithPolicyApiTest for constants.
        var expectedPolicy;
        expectedPolicy =
          {
            "chromePolicies":
              {"AllowDinosaurEasterEgg":
                {"level":"mandatory",
                 "scope":"user",
                 "source":"cloud",
                 "value":true}
              },
            "deviceLocalAccountPolicies":{},
            "extensionPolicies":{}
          }
        chrome.test.assertEq(expectedPolicy, policydata);
        chrome.test.succeed();
      }));
  },
];


var test_suites = {
  'default': defaultTests,
  'arcEnabled': arcEnabledTests,
  'enterprisePolicies': policyTests
};

chrome.test.getConfig(function(config) {
  var suite = test_suites[config.customArg];
  if (config.customArg in test_suites) {
    chrome.test.runTests(test_suites[config.customArg]);
  } else {
    chrome.test.fail('Invalid test suite');
  }
});

