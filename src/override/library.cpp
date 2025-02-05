/*
 * DISTRHO Cardinal Plugin
 * Copyright (C) 2021 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the LICENSE file.
 */

#include <library.hpp>

// Define the stuff needed for VCVRack but unused for Cardinal
namespace rack {
namespace library {

std::string appChangelogUrl;
std::string appDownloadUrl;
std::string appVersion;
std::string loginStatus;
std::map<std::string, UpdateInfo> updateInfos;
std::string updateStatus;
std::string updateSlug;
float updateProgress = 0.f;
bool isSyncing = false;
bool restartRequested = false;

void checkAppUpdate() {}
void checkUpdates() {}
bool hasUpdates() { return false; }
bool isAppUpdateAvailable() { return false; }
bool isLoggedIn() { return false; }
void logIn(const std::string&, const std::string&) {}
void logOut() {}
void syncUpdate(const std::string&) {}
void syncUpdates() {}

}
}
