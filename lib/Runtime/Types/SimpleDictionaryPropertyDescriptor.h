//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template <typename TPropertyIndex>
    class SimpleDictionaryPropertyDescriptor
    {
    public:
        SimpleDictionaryPropertyDescriptor() :
            propertyIndex(NoSlots), Attributes(PropertyDynamicTypeDefaults),
            preventFalseReference(true), isInitialized(false), isFixed(false), usedAsFixed(false) {LOGMEIN("SimpleDictionaryPropertyDescriptor.h] 14\n"); }

        SimpleDictionaryPropertyDescriptor(TPropertyIndex inPropertyIndex) :
            propertyIndex(inPropertyIndex), Attributes(PropertyDynamicTypeDefaults),
            preventFalseReference(true), isInitialized(false), isFixed(false), usedAsFixed(false) {LOGMEIN("SimpleDictionaryPropertyDescriptor.h] 18\n"); }

        SimpleDictionaryPropertyDescriptor(TPropertyIndex inPropertyIndex, PropertyAttributes attributes) :
            propertyIndex(inPropertyIndex), Attributes(attributes),
            preventFalseReference(true), isInitialized(false), isFixed(false), usedAsFixed(false) {LOGMEIN("SimpleDictionaryPropertyDescriptor.h] 22\n"); }

        // SimpleDictionaryPropertyDescriptor is allocated by a dictionary along with the PropertyRecord
        // so it can not allocate as leaf, tag the lower bit to prevent false reference
        bool preventFalseReference:1;
        bool isInitialized: 1;
        bool isFixed:1;
        bool usedAsFixed:1;

        PropertyAttributes Attributes;
        TPropertyIndex propertyIndex;

        bool HasNonLetConstGlobal() const
        {LOGMEIN("SimpleDictionaryPropertyDescriptor.h] 35\n");
            return (this->Attributes & PropertyLetConstGlobal) == 0;
        }

     private:
        static const TPropertyIndex NoSlots = PropertyIndexRanges<TPropertyIndex>::NoSlots;
    };
}

namespace JsUtil
{
    template <typename TPropertyIndex>
    class ValueEntry<Js::SimpleDictionaryPropertyDescriptor<TPropertyIndex> >: public BaseValueEntry<Js::SimpleDictionaryPropertyDescriptor<TPropertyIndex>>
    {
    public:
        void Clear()
        {LOGMEIN("SimpleDictionaryPropertyDescriptor.h] 51\n");
            this->value = 0;
        }
    };
}
