//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeBasePch.h"

namespace Js
{

PropertyManager::PropertyManager()
{
    this->propertyMap = HeapNew(PropertyMap, &HeapAllocator::Instance, TotalNumberOfBuiltInProperties + 700);
}

PropertyManager::~PropertyManager()
{
    if (this->propertyMap != nullptr)
    {
        HeapDelete(this->propertyMap);
    }
    this->propertyMap = nullptr;
}

inline bool IsDirectPropertyName(const char16 * propertyName, int propertyNameLength)
{
    return ((propertyNameLength == 1) && ((propertyName[0] & 0xFF80) == 0));
}

bool PropertyManager::TryGetValueAt(Js::PropertyId propertyId, const Js::PropertyRecord ** propertyRecord)
{
    int32 propertyIndex = propertyId - Js::PropertyIds::_none;
    return propertyMap->TryGetValueAt(propertyIndex, propertyRecord);
}

const Js::PropertyRecord * PropertyManager::LookupWithKey(const char16 * buffer, const int length)
{
    return propertyMap->LookupWithKey(Js::HashedCharacterBuffer<char16>(buffer, length));
}

void PropertyManager::Remove(const Js::PropertyRecord * propertyRecord)
{
    propertyMap->Remove(propertyRecord);
}

Js::PropertyId PropertyManager::GetNextPropertyId()
{
    return this->propertyMap->GetNextIndex() + Js::PropertyIds::_none;
}

Js::PropertyId PropertyManager::GetMaxPropertyId()
{
    return this->Count() + Js::InternalPropertyIds::Count;
}

uint PropertyManager::Count()
{
    return propertyMap->Count();
}

uint PropertyManager::GetLastIndex()
{
    return propertyMap->GetLastIndex();
}

void PropertyManager::LockResize()
{
    propertyMap->LockResize();
}

void PropertyManager::UnlockResize()
{
    propertyMap->UnlockResize();
}

void PropertyManager::EnsureCapacity()
{
    propertyMap->EnsureCapacity();
}

void PropertyManager::Add(const Js::PropertyRecord * propertyRecord)
{
    propertyMap->Add(propertyRecord);
}

} // namespace Js
