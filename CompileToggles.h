/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

namespace android {

// Provide detailed information about struct and enum values in Java. In C++, these functions are
// in headers so that only clients that need them have to absorb the function size cost. However,
// no such option is available in Java, so it is guarded globally.
#ifdef HIDL_DETAILED_JAVA_TO_STRING
constexpr static bool kDetailJavaToString = true;
#else
constexpr static bool kDetailJavaToString = false;
#endif

}  // android
