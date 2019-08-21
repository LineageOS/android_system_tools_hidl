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
#include <android-base/parseint.h>
#include <hidl-util/FQName.h>
#include <hidl-util/Formatter.h>
#include <hidl-util/StringHelper.h>
#include <cstddef>
#include <vector>

#include "AidlHelper.h"
#include "Coordinator.h"
#include "Interface.h"
#include "Method.h"
#include "NamedType.h"
#include "Reference.h"
#include "Type.h"

namespace android {

static void emitAidlMethodParams(Formatter& out, const std::vector<NamedReference<Type>*> args,
                                 const std::string& prefix) {
    out.join(args.begin(), args.end(), ", ", [&](const NamedReference<Type>* arg) {
        out << prefix << AidlHelper::getAidlType(*arg->get()) << " " << arg->name();
    });
}

std::vector<const Method*> AidlHelper::getUserDefinedMethods(const Interface& interface) {
    std::vector<const Method*> methods;
    for (const Interface* iface : interface.typeChain()) {
        const std::vector<Method*> userDefined = iface->userDefinedMethods();
        methods.insert(methods.end(), userDefined.begin(), userDefined.end());
    }

    return methods;
}

struct MethodWithVersion {
    size_t major;
    size_t minor;
    const Method* method;
    std::string name;
};

static void pushVersionedMethodOntoMap(MethodWithVersion versionedMethod,
                                       std::map<std::string, MethodWithVersion>* map,
                                       std::vector<const Method*>* ignored) {
    const Method* method = versionedMethod.method;
    std::string name = method->name();
    size_t underscore = name.find("_");
    if (underscore != std::string::npos) {
        std::string version = name.substr(underscore + 1);  // don't include _
        std::string nameWithoutVersion = name.substr(0, underscore);
        underscore = version.find("_");

        size_t major, minor;
        if (underscore != std::string::npos &&
            base::ParseUint(version.substr(0, underscore), &major) &&
            base::ParseUint(version.substr(underscore + 1), &minor)) {
            // contains major and minor version. consider it's nameWithoutVersion now.
            name = nameWithoutVersion;
            versionedMethod.name = nameWithoutVersion;
        }
    }

    // push name onto map
    auto [it, inserted] = map->emplace(std::move(name), versionedMethod);
    if (!inserted) {
        auto* current = &it->second;

        // Method in the map is more recent
        if ((current->major > versionedMethod.major) ||
            (current->major == versionedMethod.major && current->minor > versionedMethod.minor)) {
            // ignoring versionedMethod
            ignored->push_back(versionedMethod.method);
            return;
        }

        // Either current.major < versioned.major OR versioned.minor >= current.minor
        ignored->push_back(current->method);
        *current = std::move(versionedMethod);
    }
}

void AidlHelper::emitAidl(const Interface& interface, const Coordinator& coordinator) {
    for (const NamedType* type : interface.getSubTypes()) {
        emitAidl(*type, coordinator);
    }

    Formatter out = getFileWithHeader(interface, coordinator);

    interface.emitDocComment(out);
    if (interface.superType() && interface.superType()->fqName() != gIBaseFqName) {
        out << "// Interface inherits from " << interface.superType()->fqName().string()
            << " but AIDL does not support interface inheritance.\n";
    }

    out << "interface " << getAidlName(interface.fqName()) << " ";
    out.block([&] {
        for (const NamedType* type : interface.getSubTypes()) {
            emitAidl(*type, coordinator);
        }

        std::map<std::string, MethodWithVersion> methodMap;
        std::vector<const Method*> ignoredMethods;
        for (const Interface* iface : interface.typeChain()) {
            const std::vector<Method*> userDefined = iface->userDefinedMethods();
            for (const Method* method : iface->userDefinedMethods()) {
                pushVersionedMethodOntoMap(
                        {iface->fqName().getPackageMajorVersion(),
                         iface->fqName().getPackageMinorVersion(), method, method->name()},
                        &methodMap, &ignoredMethods);
            }
        }

        out.join(ignoredMethods.begin(), ignoredMethods.end(), "\n", [&](const Method* method) {
            out << "// Ignoring method " << method->name()
                << " since a newer alternative is available.";
        });
        if (!ignoredMethods.empty()) out << "\n\n";

        out.join(methodMap.begin(), methodMap.end(), "\n", [&](const auto& pair) {
            const Method* method = pair.second.method;
            method->emitDocComment(out);

            std::vector<NamedReference<Type>*> results;
            for (NamedReference<Type>* res : method->results()) {
                if (StringHelper::EndsWith(StringHelper::Uppercase(res->name()), "STATUS") ||
                    StringHelper::EndsWith(StringHelper::Uppercase(res->name()), "ERROR")) {
                    out << "// Ignoring result " << getAidlType(*res->get()) << " " << res->name()
                        << " since AIDL has built in status types.\n";
                } else {
                    results.push_back(res);
                }
            }

            if (method->name() != pair.second.name) {
                out << "// Changing method name from " << method->name() << " to "
                    << pair.second.name << "\n";
            }

            std::string returnType = "void";
            if (results.size() == 1) {
                returnType = getAidlType(*results[0]->get());

                out << "// Adding return type to method instead of out param " << returnType << " "
                    << results[0]->name() << " since there is only one return value.\n";
                results.clear();
            }

            if (method->isOneway()) out << "oneway ";
            out << returnType << " " << pair.second.name << "(";
            emitAidlMethodParams(out, method->args(), "in ");

            // Join these
            if (!results.empty()) {
                // TODO: Emit warning if a primitive is given as a out param.
                if (!method->args().empty()) out << ", ";
                emitAidlMethodParams(out, results, "out ");
            }

            out << ");\n";
        });
    });
}

}  // namespace android
