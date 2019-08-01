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

#include <hidl-util/FQName.h>
#include <hidl-util/Formatter.h>
#include <hidl-util/StringHelper.h>
#include <string>

#include "AidlHelper.h"
#include "NamedType.h"

namespace android {

std::string AidlHelper::getAidlName(const FQName& fqName) {
    std::vector<std::string> names;
    for (const std::string& name : fqName.names()) {
        names.push_back(StringHelper::Capitalize(name));
    }
    return StringHelper::JoinStrings(names, "");
}

std::string AidlHelper::getAidlName(const NamedType& type) {
    return getAidlName(type.fqName());
}

void AidlHelper::emitFileHeader(Formatter& out, const NamedType& type) {
    // TODO: Add imports

    out << "// FIXME: license file if you have one\n\n";
    out << "// TODO(hidl2aidl): Add imports\n\n";
    out << "package " << type.fqName().javaPackage() << ";\n\n";
}

}  // namespace android
