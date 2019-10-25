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

#include <android-base/logging.h>
#include <android-base/strings.h>
#include <hidl-util/FQName.h>
#include <hidl-util/Formatter.h>
#include <hidl-util/StringHelper.h>
#include <set>
#include <string>
#include <vector>

#include "AidlHelper.h"
#include "Coordinator.h"
#include "Interface.h"
#include "Method.h"
#include "NamedType.h"
#include "Reference.h"
#include "Scope.h"

namespace android {

Formatter* AidlHelper::notesFormatter = nullptr;

Formatter& AidlHelper::notes() {
    CHECK(notesFormatter != nullptr);
    return *notesFormatter;
}

void AidlHelper::setNotes(Formatter* formatter) {
    CHECK(formatter != nullptr);
    notesFormatter = formatter;
}

std::string AidlHelper::getAidlName(const FQName& fqName) {
    std::vector<std::string> names;
    for (const std::string& name : fqName.names()) {
        names.push_back(StringHelper::Capitalize(name));
    }
    return StringHelper::JoinStrings(names, "");
}

std::string AidlHelper::getAidlPackage(const FQName& fqName) {
    std::string aidlPackage = fqName.package();
    if (fqName.getPackageMajorVersion() != 1) {
        aidlPackage += std::to_string(fqName.getPackageMajorVersion());
    }

    return aidlPackage;
}

std::string AidlHelper::getAidlFQName(const FQName& fqName) {
    return getAidlPackage(fqName) + "." + getAidlName(fqName);
}

static void importNamedType(Formatter& out, const NamedType& namedType,
                            std::set<std::string>& imports) {
    std::string import = AidlHelper::getAidlFQName(namedType.fqName());
    if (imports.find(import) == imports.end()) {
        out << "import " << import << ";\n";
        imports.insert(import);
    }
}

void AidlHelper::emitFileHeader(Formatter& out, const NamedType& type) {
    out << "// FIXME: license file if you have one\n\n";
    out << "package " << getAidlPackage(type.fqName()) << ";\n\n";

    std::set<std::string> imports;
    imports.insert(getAidlFQName(gIBaseFqName));

    // Import all the defined types since they will now be in a different file
    if (type.isScope()) {
        const Scope& scope = static_cast<const Scope&>(type);
        for (const NamedType* namedType : scope.getSubTypes()) {
            importNamedType(out, *namedType, imports);
        }
    }

    // Import all the referenced types
    if (type.isInterface()) {
        // This is a separate case becase getReferences doesn't correctly traverse all the
        // superTypes and sometimes includes references to types that would not exist on AIDL
        const std::vector<const Method*>& methods =
                getUserDefinedMethods(static_cast<const Interface&>(type));
        for (const Method* method : methods) {
            for (const Reference<Type>* ref : method->getReferences()) {
                if (ref->get()->isNamedType()) {
                    importNamedType(out, *static_cast<const NamedType*>(ref->get()), imports);
                }
            }
        }
    } else {
        for (const Reference<Type>* ref : type.getReferences()) {
            if (ref->get()->isNamedType()) {
                importNamedType(out, *static_cast<const NamedType*>(ref->get()), imports);
            }
        }
    }

    // anything other than implicit IBase import
    if (imports.size() > 1) {
        out << "\n";
    }
}

Formatter AidlHelper::getFileWithHeader(const NamedType& namedType,
                                        const Coordinator& coordinator) {
    std::string aidlPackage = getAidlPackage(namedType.fqName());
    Formatter out = coordinator.getFormatter(namedType.fqName(), Coordinator::Location::DIRECT,
                                             base::Join(base::Split(aidlPackage, "."), "/") + "/" +
                                                     getAidlName(namedType.fqName()) + ".aidl");
    emitFileHeader(out, namedType);
    return out;
}

}  // namespace android
