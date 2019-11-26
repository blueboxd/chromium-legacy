// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Id for tracking automatic refresh of crash list.  */
let refreshCrashListId = undefined;

/**
 * Requests the list of crashes from the backend.
 */
function requestCrashes() {
  chrome.send('requestCrashList');
}

/**
 * Callback from backend with the list of crashes. Builds the UI.
 * @param {boolean} enabled Whether or not crash reporting is enabled.
 * @param {boolean} dynamicBackend Whether the crash backend is dynamic.
 * @param {boolean} manualUploads Whether the manual uploads are supported.
 * @param {array} crashes The list of crashes.
 * @param {string} version The browser version.
 * @param {string} os The OS name and version.
 * @param {boolean} isGoogleAccount whether primary account is internal.
 */
function updateCrashList(
    enabled, dynamicBackend, manualUploads,
    crashes, version, os, isGoogleAccount) {
  $('countBanner').textContent =
      loadTimeData.getStringF('crashCountFormat',
                              crashes.length.toLocaleString());

  const crashSection = $('crashList');

  $('disabledMode').hidden = enabled;
  $('crashUploadStatus').hidden = !enabled || !dynamicBackend;

  // Make the height fixed while clearing the
  // element in order to maintain scroll position.
  crashSection.style.height = getComputedStyle(crashSection).height;
  // Clear any previous list.
  crashSection.textContent = '';

  const productName = loadTimeData.getString('shortProductName');

  for (let i = 0; i < crashes.length; i++) {
    const crash = crashes[i];
    if (crash.local_id == '') {
      crash.local_id = productName;
    }

    const crashBlock = document.createElement('div');
    if (crash.state != 'uploaded') {
      crashBlock.className = 'notUploaded';
    }

    const title = document.createElement('h3');
    const uploaded = crash.state == 'uploaded';
    if (uploaded) {
      const crashHeaderText = loadTimeData.getString('crashHeaderFormat');
      const pieces = loadTimeData
                         .getSubstitutedStringPieces(
                             crashHeaderText, crash.id, crash.local_id)
                         .map(piece => {
                           // Create crash/ link for Googler Accounts.
                           if (isGoogleAccount && piece.value === crash.id) {
                             const crashLink = document.createElement('a');
                             crashLink.href = `http://crash/${crash.id}`;
                             crashLink.target = '_blank';
                             crashLink.textContent = crash.id;
                             return crashLink;
                           } else {
                             return piece.value;
                           }
                         });
      title.append.apply(title, pieces);
    } else {
      title.textContent = loadTimeData.getStringF('crashHeaderFormatLocalOnly',
                                                  crash.local_id);
    }
    crashBlock.appendChild(title);

    if (uploaded) {
      const date = document.createElement('p');
      date.textContent = '';
      if (crash.capture_time) {
        date.textContent += loadTimeData.getStringF(
            'crashCaptureAndUploadTimeFormat', crash.capture_time,
            crash.upload_time);
      } else {
        date.textContent += loadTimeData.getStringF('crashUploadTimeFormat',
                                                    crash.upload_time);
      }
      crashBlock.appendChild(date);

      const linkBlock = document.createElement('p');
      const link = document.createElement('a');
      const commentLines = [
        'IMPORTANT: Your crash has already been automatically reported ' +
        'to our crash system. Please file this bug only if you can provide ' +
        'more information about it.',
        '',
        '',
        'Chrome Version: ' + version,
        'Operating System: ' + os,
        '',
        'URL (if applicable) where crash occurred:',
        '',
        'Can you reproduce this crash?',
        '',
        'What steps will reproduce this crash? (If it\'s not ' +
        'reproducible, what were you doing just before the crash?)',
        '1.', '2.', '3.',
        '',
        '****DO NOT CHANGE BELOW THIS LINE****',
        'Crash ID: crash/' + crash.id
      ];
      const params = {
        template: 'Crash Report',
        comment: commentLines.join('\n'),
        // TODO(scottmg): Use add_labels to add 'User-Submitted' rather than
        // duplicating the template's labels (the first two) once
        // https://bugs.chromium.org/p/monorail/issues/detail?id=1488 is done.
        labels: 'Restrict-View-EditIssue,Stability-Crash,User-Submitted',
      };
      let href = 'https://code.google.com/p/chromium/issues/entry';
      for (const param in params) {
        href = appendParam(href, param, params[param]);
      }
      link.href = href;
      link.target = '_blank';
      link.textContent = loadTimeData.getString('bugLinkText');
      linkBlock.appendChild(link);
      crashBlock.appendChild(linkBlock);
    } else {
      let textContentKey;
      if (crash.state == 'pending_user_requested') {
        textContentKey = 'crashUserRequested';
      } else if (crash.state == 'pending') {
        textContentKey = 'crashPending';
      } else if (crash.state == 'not_uploaded') {
        textContentKey = 'crashNotUploaded';
      } else {
        continue;
      }

      const crashText = document.createElement('p');
      crashText.textContent = loadTimeData.getStringF(textContentKey,
                                                      crash.capture_time);
      crashBlock.appendChild(crashText);

      if (crash.file_size != '') {
        const crashSizeText =  document.createElement('p');
        crashSizeText.textContent = loadTimeData.getStringF('crashSizeMessage',
                                                            crash.file_size);
        crashBlock.appendChild(crashSizeText);
      }

      // Do not show "Send now" link for already requested crashes.
      if (crash.state != 'pending_user_requested' && manualUploads) {
        const uploadNowLinkBlock = document.createElement('p');
        const link = document.createElement('a');
        link.href = '';
        link.textContent = loadTimeData.getString('uploadNowLinkText');
        link.local_id = crash.local_id;
        link.onclick = function() {
          chrome.send('requestSingleCrashUpload', [this.local_id]);
        };
        uploadNowLinkBlock.appendChild(link);
        crashBlock.appendChild(uploadNowLinkBlock);
      }
    }
    crashSection.appendChild(crashBlock);
  }

  // Reset the height, in order to accommodate for the new content.
  crashSection.style.height = "";
  $('noCrashes').hidden = crashes.length != 0;
}

/**
 * Request crashes get uploaded in the background.
 */
function requestCrashUpload() {
  // Don't need locking with this call because the system crash reporter
  // has locking built into itself.
  chrome.send('requestCrashUpload');

  // Trigger a refresh in 5 seconds.  Clear any previous requests.
  clearTimeout(refreshCrashListId);
  refreshCrashListId = setTimeout(requestCrashes, 5000);
}

document.addEventListener('DOMContentLoaded', function() {
  $('uploadCrashes').onclick = requestCrashUpload;
  requestCrashes();
});
