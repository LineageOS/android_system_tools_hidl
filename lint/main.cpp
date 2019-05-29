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

#include <iostream>
#include <vector>

#include "AST.h"
#include "Coordinator.h"
#include "Lint.h"
#include "LintRegistry.h"

using namespace android;

static void usage(const char* me) {
    Formatter out(stderr);

    out << "Usage: " << me << " ";
    Coordinator::emitOptionsUsageString(out);
    out << " FQNAME...\n\n";

    out << "Process FQNAME, PACKAGE(.SUBPACKAGE)*@[0-9]+.[0-9]+(::TYPE)?, and provide lints.\n\n";

    out.indent();
    out.indent();

    out << "-h: Prints this menu.\n";
    Coordinator::emitOptionsDetailString(out);

    out.unindent();
    out.unindent();
}

int main(int argc, char** argv) {
    const char* me = argv[0];
    if (argc == 1) {
        usage(me);
        std::cerr << "ERROR: no fqname specified." << std::endl;
        exit(1);
    }

    Coordinator coordinator;

    coordinator.parseOptions(argc, argv, "h", [&](int res, char* /* arg */) {
        switch (res) {
            case 'h':
            case '?':
            default: {
                usage(me);
                exit(1);
                break;
            }
        }
    });

    argc -= optind;
    argv += optind;

    if (argc == 0) {
        usage(me);
        std::cerr << "ERROR: no fqname specified." << std::endl;
        exit(1);
    }

    for (int i = 0; i < argc; ++i) {
        const char* arg = argv[i];

        FQName fqName;
        if (!FQName::parse(arg, &fqName)) {
            std::cerr << "ERROR: Invalid fully-qualified name as argument: " << arg << "."
                      << std::endl;
            exit(1);
        }

        std::vector<FQName> targets;
        if (fqName.isFullyQualified()) {
            targets.push_back(fqName);
        } else {
            status_t err = coordinator.appendPackageInterfacesToVector(fqName, &targets);
            if (err != OK) {
                std::cerr << "ERROR: Could not get sources for: " << arg << "." << std::endl;
                exit(1);
            }
        }

        std::vector<Lint> errors;
        for (const FQName& target : targets) {
            AST* ast = coordinator.parse(target);
            if (ast == nullptr) {
                std::cerr << "ERROR: Could not parse " << target.name() << ". Aborting."
                          << std::endl;
                exit(1);
            }

            LintRegistry::get()->runAllLintFunctions(*ast, &errors);
        }

        if (!errors.empty())
            std::cerr << "Lints for: " << fqName.string() << std::endl << std::endl;
        for (const Lint& error : errors) {
            std::cerr << error;
        }
    }

    return 0;
}