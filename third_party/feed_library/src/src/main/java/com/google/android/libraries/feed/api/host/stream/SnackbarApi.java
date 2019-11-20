// Copyright 2018 The Feed Authors.
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

/** The SnackbarApi is used by the Feed to render snackbars. */
public interface SnackbarApi {
  /**
   * Displays a snackbar on the host.
   *
   * @param message Text to display in the snackbar.
   */
  void show(String message);

  /**
   * Displays a snackbar on the host.
   *
   * @param message Text to display in the snackbar.
   * @param message Text to display in the snackbar's action. e.g. "undo"
   * @param callback A Callback for the client to know why and when the snackbar goes away.
   */
  void show(String message, String action, SnackbarCallbackApi callback);
}
