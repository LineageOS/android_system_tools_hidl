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

#include <android-base/strings.h>
#include <hidl-util/StringHelper.h>

#include <algorithm>
#include <string>
#include <vector>

#include "AST.h"
#include "DocComment.h"
#include "Interface.h"
#include "Lint.h"
#include "LintRegistry.h"
#include "Method.h"
#include "Reference.h"

namespace android {

// returns true if the line contained a prefix and false otherwise
static bool getFirstWordAfterPrefix(const std::string& str, const std::string& prefix,
                                    std::string* out) {
    std::string line = base::Trim(str);
    if (base::StartsWith(line, prefix)) {
        line = StringHelper::LTrim(line, prefix);

        // check line has other stuff and that there is a space between return and the rest
        if (!base::StartsWith(line, " ")) {
            *out = "";
            return true;
        }

        line = StringHelper::LTrimAll(line, " ");

        std::vector<std::string> words = base::Split(line, " ");
        *out = words.empty() ? "" : words.at(0);
        return true;
    }

    return false;
}

static bool isNameInList(const std::string& name, const std::vector<NamedReference<Type>*>& refs) {
    return std::any_of(refs.begin(), refs.end(), [&](const NamedReference<Type>* namedRef) -> bool {
        return namedRef->name() == name;
    });
}

static void methodDocComments(const AST& ast, std::vector<Lint>* errors) {
    const Interface* iface = ast.getInterface();
    if (iface == nullptr) {
        // no interfaces so no methods
        return;
    }

    for (const Method* method : iface->isIBase() ? iface->methods() : iface->userDefinedMethods()) {
        const DocComment* docComment = method->getDocComment();
        if (docComment == nullptr) continue;

        std::vector<std::string> lines = base::Split(docComment->string(), "\n");

        // want a copy so that it can be mutated
        for (const std::string& line : lines) {
            std::string returnName;

            if (bool foundPrefix = getFirstWordAfterPrefix(line, "@return", &returnName);
                foundPrefix) {
                if (returnName.empty()) {
                    errors->push_back(Lint(WARNING, docComment->location())
                                      << "@return should be followed by a return parameter.\n");
                    continue;
                }

                if (!isNameInList(returnName, method->results())) {
                    errors->push_back(Lint(WARNING, docComment->location())
                                      << "@return " << returnName
                                      << " is not a return parameter of the method "
                                      << method->name() << ".\n");
                }

                continue;
            }

            if (bool foundPrefix = getFirstWordAfterPrefix(line, "@param", &returnName);
                foundPrefix) {
                if (returnName.empty()) {
                    errors->push_back(Lint(WARNING, docComment->location())
                                      << "@param should be followed by a parameter name.\n");
                    continue;
                }

                if (!isNameInList(returnName, method->args())) {
                    errors->push_back(Lint(WARNING, docComment->location())
                                      << "@param " << returnName
                                      << " is not an argument to the method " << method->name()
                                      << ".\n");
                }
            }
        }
    }
}

REGISTER_LINT(methodDocComments);
}  // namespace android
