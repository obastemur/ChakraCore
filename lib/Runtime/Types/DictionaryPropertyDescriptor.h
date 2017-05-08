//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template <typename TPropertyIndex>
    class DictionaryPropertyDescriptor
    {
        template <typename T>
        friend class DictionaryPropertyDescriptor;
    public:
        DictionaryPropertyDescriptor(TPropertyIndex dataSlot, bool isInitialized = false, bool isFixed = false, bool usedAsFixed = false) :
            Data(dataSlot), Getter(NoSlots), Setter(NoSlots), Attributes(PropertyDynamicTypeDefaults),
            PreventFalseReference(true), IsInitialized(isInitialized), IsOnlyOneAccessorInitialized(false), IsFixed(isFixed), UsedAsFixed(usedAsFixed), IsShadowed(false), IsAccessor(false) {TRACE_IT(65393); }

        DictionaryPropertyDescriptor(TPropertyIndex getterSlot, TPropertyIndex setterSlot, bool isInitialized = false, bool isFixed = false, bool usedAsFixed = false) :
            Data(NoSlots), Getter(getterSlot), Setter(setterSlot), Attributes(PropertyDynamicTypeDefaults),
            PreventFalseReference(true), IsInitialized(isInitialized), IsOnlyOneAccessorInitialized(false), IsFixed(isFixed), UsedAsFixed(usedAsFixed), IsShadowed(false), IsAccessor(true) {TRACE_IT(65394); }

        DictionaryPropertyDescriptor(TPropertyIndex dataSlot, PropertyAttributes attributes, bool isInitialized = false, bool isFixed = false, bool usedAsFixed = false) :
            Data(dataSlot), Getter(NoSlots), Setter(NoSlots), Attributes(attributes),
            PreventFalseReference(true), IsInitialized(isInitialized), IsOnlyOneAccessorInitialized(false), IsFixed(isFixed), UsedAsFixed(usedAsFixed), IsShadowed(false), IsAccessor(false) {TRACE_IT(65395); }

        DictionaryPropertyDescriptor(TPropertyIndex getterSlot, TPropertyIndex setterSlot, PropertyAttributes attributes, bool isInitialized = false, bool isFixed = false, bool usedAsFixed = false) :
            Data(NoSlots), Getter(getterSlot), Setter(setterSlot), Attributes(attributes),
            PreventFalseReference(true), IsInitialized(isInitialized), IsOnlyOneAccessorInitialized(false), IsFixed(isFixed), UsedAsFixed(usedAsFixed), IsShadowed(false), IsAccessor(true) { }

        // this is for initialization.
        DictionaryPropertyDescriptor() : Data(NoSlots), Getter(NoSlots), Setter(NoSlots), Attributes(PropertyDynamicTypeDefaults),
            PreventFalseReference(true), IsInitialized(false), IsOnlyOneAccessorInitialized(false), IsFixed(false), UsedAsFixed(false), IsShadowed(false), IsAccessor(false) {TRACE_IT(65396); }

        template <typename TPropertyIndexFrom>
        void CopyFrom(DictionaryPropertyDescriptor<TPropertyIndexFrom>& descriptor);

        // SimpleDictionaryPropertyDescriptor is allocated by a dictionary along with the PropertyRecord
        // so it cannot be allocated as leaf, tag the lower bit to prevent false reference.
        bool PreventFalseReference:1;

        bool IsInitialized:1;
        bool IsOnlyOneAccessorInitialized:1;
        bool IsFixed:1;
        bool UsedAsFixed:1;
        bool IsShadowed : 1;
        bool IsAccessor : 1;
        PropertyAttributes Attributes;
    private:
        TPropertyIndex Data;
        // CONSIDER: sharing the Data slot with one of these and use the attributes to tell it apart.
        TPropertyIndex Getter;
        TPropertyIndex Setter;
    public:
        template <bool allowLetConstGlobal>
        TPropertyIndex GetDataPropertyIndex() const;
        TPropertyIndex GetGetterPropertyIndex() const;
        TPropertyIndex GetSetterPropertyIndex() const;
        void ConvertToData();
        bool ConvertToGetterSetter(TPropertyIndex& nextSlotIndex);

        bool HasNonLetConstGlobal() const
        {TRACE_IT(65397);
            return (this->Attributes & PropertyLetConstGlobal) == 0 || this->IsShadowed;
        }
        void AddShadowedData(TPropertyIndex& nextPropertyIndex, bool addingLetConstGlobal);
    private:
        static const TPropertyIndex NoSlots = PropertyIndexRanges<TPropertyIndex>::NoSlots;

    public:
#if DBG
        bool SanityCheckFixedBits()
        {TRACE_IT(65398);
            return
                ((!this->IsFixed && !this->UsedAsFixed) ||
                (!(this->Attributes & PropertyDeleted) && (this->Data != NoSlots || this->Getter != NoSlots || this->Setter != NoSlots)));
        }
#endif
    };


    template <typename TPropertyIndex>
    template <bool allowLetConstGlobal>
    TPropertyIndex DictionaryPropertyDescriptor<TPropertyIndex>::GetDataPropertyIndex() const
    {TRACE_IT(65399);
        // If it is let const global, the data slot is the let const property, and if we allow let const global,
        // we already return that the Getter/Setter slot may be doubled as the Data Slot
        // so only return it if we allow let const
        bool const isLetConstGlobal = (this->Attributes & PropertyLetConstGlobal) != 0;
        if (isLetConstGlobal)
        {TRACE_IT(65400);
            Assert(this->Data != NoSlots);  // Should always have slot for LetConstGlobal if specified
            if (allowLetConstGlobal)
            {TRACE_IT(65401);
                return this->Data;
            }
            else if (this->IsShadowed && !this->IsAccessor)
            {TRACE_IT(65402);
                // if it is a let const global, if the setter slot is empty, then the Getter slot must be
                // the shadowed data slot, return that.
                return this->Getter;
            }
        }
        else
        {TRACE_IT(65403);
            Assert(!this->IsAccessor || this->Data == NoSlots);
            return this->Data;
        }
        return NoSlots;
    }

    template <typename TPropertyIndex>
    TPropertyIndex DictionaryPropertyDescriptor<TPropertyIndex>::GetGetterPropertyIndex() const
    {TRACE_IT(65404);
        // Need to check data property index first
        Assert(GetDataPropertyIndex<false>() == NoSlots);
        return this->Getter;
    }

    template <typename TPropertyIndex>
    TPropertyIndex DictionaryPropertyDescriptor<TPropertyIndex>::GetSetterPropertyIndex() const
    {TRACE_IT(65405);
        // Need to check data property index first
        Assert(GetDataPropertyIndex<false>() == NoSlots);
        return this->Setter;
    }

    template <typename TPropertyIndex>
    void DictionaryPropertyDescriptor<TPropertyIndex>::ConvertToData()
    {TRACE_IT(65406);
        if (this->IsAccessor)
        {TRACE_IT(65407);
            Assert(this->Getter != NoSlots && this->Setter != NoSlots);
            this->IsAccessor = false;
            if (this->IsShadowed)
            {TRACE_IT(65408);
                Assert(this->Data != NoSlots);
            }
            else
            {TRACE_IT(65409);
                Assert(this->Data == NoSlots);
                this->Data = this->Getter;
                this->Getter = NoSlots;
            }
        }
        Assert(GetDataPropertyIndex<false>() != NoSlots);
    }

    template <typename TPropertyIndex>
    void DictionaryPropertyDescriptor<TPropertyIndex>::AddShadowedData(TPropertyIndex& nextPropertyIndex, bool addingLetConstGlobal)
    {TRACE_IT(65410);
        Assert(!this->IsShadowed);
        this->IsShadowed = true;
        if (this->IsAccessor)
        {TRACE_IT(65411);
            Assert(this->Data == NoSlots);
        }
        else if (addingLetConstGlobal)
        {TRACE_IT(65412);
            this->Getter = this->Data;
            this->Data = nextPropertyIndex++;
        }
        else
        {TRACE_IT(65413);
            this->Getter = nextPropertyIndex++;
        }
        this->Attributes |= PropertyLetConstGlobal;
        Assert(GetDataPropertyIndex<false>() != NoSlots);
    }

    template <typename TPropertyIndex>
    bool DictionaryPropertyDescriptor<TPropertyIndex>::ConvertToGetterSetter(TPropertyIndex& nextPropertyIndex)
    {TRACE_IT(65414);
        // Initial descriptor state and corresponding conversion can be one of the following:
        //
        // | State                              | Data    | Getter   | Setter   | Conversion                                                                                        |
        // |------------------------------------|---------|----------|----------|---------------------------------------------------------------------------------------------------|
        // | Data property                      | valid   | NoSlots? | NoSlots? | Move Data to Getter, set Data to NoSlots, create new slot for Setter if necessary, set IsAccessor |
        // | LetConstGlobal                     | valid   | NoSlots? | NoSlots? | Create new slots for Getter and Setter if necessary, set IsAccessor, set IsShadowed               |
        // | Data property + LetConstGlobal     | valid   | valid    | NoSlots? | Create new slot for Setter if necessary, set IsAccessor                                           |
        // | Accessor property                  | NoSlots | valid    | valid    | Nothing                                                                                           |
        // | Accessor property + LetConstGlobal | valid   | valid    | valid    | Nothing                                                                                           |
        // |------------------------------------|---------|----------|----------|---------------------------------------------------------------------------------------------------|
        //
        // NOTE: Do not create slot for Getter/Setter if they are already valid; possible after previous conversion from Accessor to Data, or deletion of Accessor, etc.

        if (this->IsAccessor)
        {TRACE_IT(65415);
            // Accessor property
            // Accessor property + LetConstGlobal
            Assert(this->Getter != NoSlots && this->Setter != NoSlots);
            return false;
        }

        this->IsAccessor = true;
        if (this->Attributes & PropertyLetConstGlobal)
        {TRACE_IT(65416);
            if (this->IsShadowed)
            {TRACE_IT(65417);
                // Data property + LetConstGlobal
                Assert(this->Getter != NoSlots);
            }
            else
            {TRACE_IT(65418);
                // LetConstGlobal
                this->IsShadowed = true;
            }
        }
        else
        {TRACE_IT(65419);
            // Data property
            Assert(this->Data != NoSlots);
            Assert(this->Getter == NoSlots);
            this->Getter = this->Data;
            this->Data = NoSlots;
        }

        bool addedPropertyIndex = false;
        if (this->Getter == NoSlots)
        {TRACE_IT(65420);
            this->Getter = nextPropertyIndex++;
            addedPropertyIndex = true;
        }
        if (this->Setter == NoSlots)
        {TRACE_IT(65421);
            this->Setter = nextPropertyIndex++;
            addedPropertyIndex = true;
        }
        Assert(this->GetGetterPropertyIndex() != NoSlots || this->GetSetterPropertyIndex() != NoSlots);
        return addedPropertyIndex;
    }

    template <typename TPropertyIndex>
    template <typename TPropertyIndexFrom>
    void DictionaryPropertyDescriptor<TPropertyIndex>::CopyFrom(DictionaryPropertyDescriptor<TPropertyIndexFrom>& descriptor)
    {TRACE_IT(65422);
        this->Attributes = descriptor.Attributes;
        this->Data = (descriptor.Data == DictionaryPropertyDescriptor<TPropertyIndexFrom>::NoSlots) ? NoSlots : descriptor.Data;
        this->Getter = (descriptor.Getter == DictionaryPropertyDescriptor<TPropertyIndexFrom>::NoSlots) ? NoSlots : descriptor.Getter;
        this->Setter = (descriptor.Setter == DictionaryPropertyDescriptor<TPropertyIndexFrom>::NoSlots) ? NoSlots : descriptor.Setter;

        this->IsInitialized = descriptor.IsInitialized;
        this->IsFixed = descriptor.IsFixed;
        this->UsedAsFixed = descriptor.UsedAsFixed;
        this->IsAccessor = descriptor.IsAccessor;
    }
}
