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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <hidl-util/StringHelper.h>

#include <string>
#include <vector>

#include "../Lint.h"
#include "../LintRegistry.h"
#include "Coordinator.h"

using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::Property;

namespace android {
class HidlLintTest : public ::testing::Test {
  protected:
    Coordinator coordinator;

    void SetUp() override {
        char* argv[2] = {const_cast<char*>("hidl-lint"),
                         const_cast<char*>("-rlint_test:system/tools/hidl/lint/test/interfaces")};
        coordinator.parseOptions(2, argv, "", [](int /* opt */, char* /* optarg */) {});
    }

    void getLintsForHal(const std::string& name, std::vector<Lint>* errors) {
        std::vector<FQName> targets;

        FQName fqName;
        if (!FQName::parse(name, &fqName)) {
            FAIL() << "Could not parse fqName: " << name;
        }

        if (fqName.isFullyQualified()) {
            targets.push_back(fqName);
        } else {
            status_t err = coordinator.appendPackageInterfacesToVector(fqName, &targets);
            if (err != OK) {
                FAIL() << "Could not get sources for: " << name;
            }
        }

        for (const FQName& fqName : targets) {
            AST* ast = coordinator.parse(fqName);
            if (ast == nullptr) {
                FAIL() << "Could not parse " << fqName.name() << ". Aborting.";
            }

            LintRegistry::get()->runAllLintFunctions(*ast, errors);
        }
    }
};

#define EXPECT_NO_LINT(interface)           \
    do {                                    \
        std::vector<Lint> errors;           \
        getLintsForHal(interface, &errors); \
        EXPECT_EQ(0, errors.size());        \
    } while (false)

#define EXPECT_LINT(interface, errorMsg)                              \
    do {                                                              \
        std::vector<Lint> errors;                                     \
        getLintsForHal(interface, &errors);                           \
        EXPECT_EQ(1, errors.size());                                  \
        if (errors.size() != 1) break;                                \
        EXPECT_THAT(errors[0].getMessage(), ContainsRegex(errorMsg)); \
    } while (false)

#define EXPECT_A_LINT(interface, errorMsg)                                                   \
    do {                                                                                     \
        std::vector<Lint> errors;                                                            \
        getLintsForHal(interface, &errors);                                                  \
        EXPECT_LE(1, errors.size());                                                         \
        if (errors.size() < 1) break;                                                        \
        EXPECT_THAT(errors, Contains(Property(&Lint::getMessage, ContainsRegex(errorMsg)))); \
    } while (false)

TEST_F(HidlLintTest, OnewayLintTest) {
    // Has no errors (empty). Lint size should be 0.
    EXPECT_NO_LINT("lint_test.oneway@1.0::IEmpty");

    // Only has either oneway or nononeway methods. Lint size should be 0.
    EXPECT_NO_LINT("lint_test.oneway@1.0::IOneway");
    EXPECT_NO_LINT("lint_test.oneway@1.0::INonOneway");

    // A child of a mixed interface should not trigger a lint if it is oneway/nononeway.
    // Lint size should be 0
    EXPECT_NO_LINT("lint_test.oneway@1.0::IMixedOnewayChild");
    EXPECT_NO_LINT("lint_test.oneway@1.0::IMixedNonOnewayChild");

    // A child with the same oneway type should not trigger a lint. Lint size should be 0.
    EXPECT_NO_LINT("lint_test.oneway@1.0::IOnewayChild");
    EXPECT_NO_LINT("lint_test.oneway@1.0::INonOnewayChild");

    // This interface is mixed. Should have a lint.
    EXPECT_LINT("lint_test.oneway@1.0::IMixed", "IMixed has both oneway and non-oneway methods.");

    // Regardless of parent, if interface is mixed, it should have a lint.
    EXPECT_LINT("lint_test.oneway@1.0::IMixedMixedChild",
                "IMixedMixedChild has both oneway and non-oneway methods.");

    // When onewaytype is different from parent it should trigger a lint.
    EXPECT_LINT("lint_test.oneway@1.0::IOnewayOpposite",
                "IOnewayOpposite should only have oneway methods");

    EXPECT_LINT("lint_test.oneway@1.0::INonOnewayOpposite",
                "INonOnewayOpposite should only have non-oneway methods");
}

TEST_F(HidlLintTest, SafeunionLintTest) {
    // Has no errors (empty). Even though types.hal has a lint.
    EXPECT_NO_LINT("lint_test.safeunion@1.0::IEmpty");

    // A child of an interface that refers to a union should not lint unless it refers to a union
    EXPECT_NO_LINT("lint_test.safeunion@1.1::IReference");

    // Should lint the union type definition
    EXPECT_LINT("lint_test.safeunion@1.0::types", "union InTypes.*defined");
    EXPECT_LINT("lint_test.safeunion@1.0::IDefined", "union SomeUnion.*defined");

    // Should mention that a union type is being referenced and where that type is.
    EXPECT_LINT("lint_test.safeunion@1.0::IReference", "Reference to union type.*types.hal");

    // Referencing a union inside a struct should lint
    EXPECT_LINT("lint_test.safeunion@1.1::types", "Reference to union type.*1\\.0/types.hal");

    // Defining a union inside a struct should lint
    EXPECT_LINT("lint_test.safeunion@1.0::IUnionInStruct", "union SomeUnionInStruct.*defined");

    // Reference to a struct that contains a union should lint
    EXPECT_LINT("lint_test.safeunion@1.1::IReferStructWithUnion",
                "Reference to struct.*contains a union type.");
}

TEST_F(HidlLintTest, ImportTypesTest) {
    // Imports types.hal file from package
    EXPECT_LINT("lint_test.import_types@1.0::IImport", "Redundant import");

    // Imports types.hal from other package
    EXPECT_LINT("lint_test.import_types@1.0::IImportOther", "This imports every type");

    // Imports types.hal from previous version of the same package
    EXPECT_LINT("lint_test.import_types@1.1::types", "This imports every type");

    // Imports types.hal from same package with fully qualified name
    EXPECT_LINT("lint_test.import_types@1.1::IImport", "Redundant import");
}

TEST_F(HidlLintTest, SmallStructsTest) {
    // Referencing bad structs should not lint
    EXPECT_NO_LINT("lint_test.small_structs@1.0::IReference");

    // Empty structs/unions should lint
    EXPECT_LINT("lint_test.small_structs@1.0::IEmptyStruct", "contains no elements");
    EXPECT_A_LINT("lint_test.small_structs@1.0::IEmptyUnion", "contains no elements");

    // Structs/unions with single field should lint
    EXPECT_LINT("lint_test.small_structs@1.0::ISingleStruct", "only contains 1 element");
    EXPECT_A_LINT("lint_test.small_structs@1.0::ISingleUnion", "only contains 1 element");
}

TEST_F(HidlLintTest, DocCommentRefTest) {
    EXPECT_NO_LINT("lint_test.doc_comments@1.0::ICorrect");

    // Should lint since nothing follows the keyword
    EXPECT_LINT("lint_test.doc_comments@1.0::INoReturn",
                "should be followed by a return parameter");
    EXPECT_LINT("lint_test.doc_comments@1.0::INoParam", "should be followed by a parameter name");
    EXPECT_LINT("lint_test.doc_comments@1.0::IReturnSpace",
                "should be followed by a return parameter");

    // Typos should be caught
    EXPECT_LINT("lint_test.doc_comments@1.0::IWrongReturn", "is not a return parameter");
    EXPECT_LINT("lint_test.doc_comments@1.0::IWrongParam", "is not an argument");

    // Incorrectly marked as @param should lint as a param
    EXPECT_LINT("lint_test.doc_comments@1.0::ISwitched", "is not an argument");
}
}  // namespace android