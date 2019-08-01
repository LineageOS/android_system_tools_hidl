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

#include <iostream>
#include <vector>

#include "AST.h"
#include "AidlHelper.h"
#include "Coordinator.h"

using namespace android;

static void usage(const char* me) {
    Formatter out(stderr);

    out << "Usage: " << me << " [-o <output path>] ";
    Coordinator::emitOptionsUsageString(out);
    out << " FQNAME...\n\n";

    out << "Converts FQNAME, PACKAGE(.SUBPACKAGE)*@[0-9]+.[0-9]+(::TYPE)? to an aidl "
           "equivalent.\n\n";

    out.indent();
    out.indent();

    out << "-o <output path>: Location to output files.\n";
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
    std::string outputPath;
    coordinator.parseOptions(argc, argv, "ho:", [&](int res, char* arg) {
        switch (res) {
            case 'o': {
                if (!outputPath.empty()) {
                    fprintf(stderr, "ERROR: -o <output path> can only be specified once.\n");
                    exit(1);
                }
                outputPath = arg;
                break;
            }
            case 'h':
            case '?':
            default: {
                usage(me);
                exit(1);
                break;
            }
        }
    });

    if (outputPath.back() != '/') {
        outputPath += "/";
    }
    coordinator.setOutputPath(outputPath);

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

        for (const FQName& target : targets) {
            AST* ast = coordinator.parse(target);
            if (ast == nullptr) {
                std::cerr << "ERROR: Could not parse " << target.name() << ". Aborting."
                          << std::endl;
                exit(1);
            }

            const Interface* iface = ast->getInterface();
            if (iface) {
                AidlHelper::emitAidl(*iface, coordinator);
            } else {
                AidlHelper::emitAidl(ast->getRootScope(), coordinator);
            }
        }
    }

    return 0;
}
