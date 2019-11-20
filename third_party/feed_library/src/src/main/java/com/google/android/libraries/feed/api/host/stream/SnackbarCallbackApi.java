// Copyright 2019 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.api.host.stream;

/** The SnackbarCallbackApi is a Callback class for Snackbar events. */
public interface SnackbarCallbackApi {

  /** Called when the user clicks the action button on the snackbar. */
  void onDismissedWithAction();

  /** Called when the snackbar is dismissed by timeout or UI environment change. */
  void onDismissNoAction();
}
