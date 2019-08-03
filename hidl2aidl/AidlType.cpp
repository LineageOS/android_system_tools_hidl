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

#include <string>

#include "AidlHelper.h"
#include "NamedType.h"
#include "Type.h"
#include "VectorType.h"

namespace android {

std::string AidlHelper::getAidlType(const Type& type) {
    if (type.isVector()) {
        const VectorType& vec = static_cast<const VectorType&>(type);
        const Type* elementType = vec.getElementType();

        // Aidl doesn't support List<*> for C++ and NDK backends
        return AidlHelper::getAidlType(*elementType) + "[]";
    } else if (type.isNamedType()) {
        const NamedType& namedType = static_cast<const NamedType&>(type);
        return AidlHelper::getAidlFQName(namedType.fqName());
    } else {
        return type.getJavaType();
    }
}

}  // namespace android
