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
#include <hidl-util/FQName.h>
#include <hidl-util/Formatter.h>
#include <hidl-util/StringHelper.h>
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

void AidlHelper::emitAidl(const Interface& interface, const Coordinator& coordinator) {
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

        const std::vector<const Method*>& methods = getUserDefinedMethods(interface);
        out.join(methods.begin(), methods.end(), "\n", [&](const Method* method) {
            method->emitDocComment(out);

            std::vector<NamedReference<Type>*> results;
            for (NamedReference<Type>* res : method->results()) {
                if (StringHelper::EndsWith(StringHelper::Uppercase(res->name()), "STATUS") ||
                    StringHelper::EndsWith(StringHelper::Uppercase(res->name()), "ERROR")) {
                    out << "// Ignoring result " << res->get()->getJavaType() << " " << res->name()
                        << " since AIDL has built in status types.\n";
                } else {
                    results.push_back(res);
                }
            }

            std::string returnType = "void";
            if (results.size() == 1) {
                returnType = getAidlType(*results[0]->get());

                out << "// Adding return type to method instead of out param "
                    << results[0]->get()->getJavaType() << " " << results[0]->name()
                    << " since there is only one return value.\n";
                results.clear();
            }

            if (method->isOneway()) out << "oneway ";
            out << returnType << " " << method->name() << "(";
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
