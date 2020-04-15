/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package dev.hadrosaur.vulkanphotobooth

import android.content.SharedPreferences
import android.preference.PreferenceManager

/**
 * Convenience class for reading/writing shared preferences
 */
class PrefHelper {
    companion object {
        internal fun getLastVersion(activity: MainActivity): Int {
            val sharedPref: SharedPreferences =
                PreferenceManager.getDefaultSharedPreferences(activity)
            return sharedPref.getInt(activity.getString(R.string.settings_last_version_key), 0)
        }
        internal fun setLastVersion(activity: MainActivity, version: Int) {
            val prefEditor = PreferenceManager.getDefaultSharedPreferences(activity).edit()
            prefEditor.putInt(activity.getString(R.string.settings_last_version_key), version)
            prefEditor.apply()
        }
    }
}