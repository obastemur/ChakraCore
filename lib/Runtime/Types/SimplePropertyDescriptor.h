//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class SimplePropertyDescriptor
    {
    public:
        SimplePropertyDescriptor() : Id(nullptr), preventFalseReference(nullptr) {LOGMEIN("SimplePropertyDescriptor.h] 11\n"); Attributes = PropertyDynamicTypeDefaults; }
        SimplePropertyDescriptor(const PropertyRecord* id) : Id(id), preventFalseReference(NULL) {LOGMEIN("SimplePropertyDescriptor.h] 12\n"); Attributes = PropertyDynamicTypeDefaults; }
        SimplePropertyDescriptor(const PropertyRecord* id, PropertyAttributes attributes) : Id(id), preventFalseReference(NULL) {LOGMEIN("SimplePropertyDescriptor.h] 13\n"); Attributes = attributes; }

        SimplePropertyDescriptor(const PropertyRecord* id, _no_write_barrier_tag, PropertyAttributes attributes)
            : Id(NO_WRITE_BARRIER_TAG(id)), preventFalseReference(NULL)
        {LOGMEIN("SimplePropertyDescriptor.h] 17\n"); Attributes = attributes; }

        Field(const PropertyRecord*) Id;
        union
        {
            Field(PropertyAttributes) Attributes;
            FieldNoBarrier(void*) preventFalseReference; // SimplePropertyDescriptor can be declared on stack. Always zero out to avoid this becoming a memory address reference.
        };
    };

    CompileAssert(sizeof(SimplePropertyDescriptor) == 2 * sizeof(Var));
}
