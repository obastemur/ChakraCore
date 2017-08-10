//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class PropertyManager
    {
    public:
        typedef JsUtil::BaseHashSet<const Js::PropertyRecord *, HeapAllocator, PrimeSizePolicy, const Js::PropertyRecord *,
            Js::PropertyRecordStringHashComparer, JsUtil::SimpleHashedEntry, JsUtil::AsymetricResizeLock> PropertyMap;
        PropertyMap * propertyMap;
        PropertyManager();
        ~PropertyManager();

        bool TryGetValueAt(Js::PropertyId propertyId, const Js::PropertyRecord ** propertyRecord);
        const Js::PropertyRecord * LookupWithKey(const char16 * buffer, const int length);
        void Remove(const Js::PropertyRecord * propertyRecord);
        Js::PropertyId GetNextPropertyId();
        Js::PropertyId GetMaxPropertyId();
        uint Count();
        uint GetLastIndex();

        void LockResize();
        void UnlockResize();

        void EnsureCapacity();
        void Add(const Js::PropertyRecord * propertyRecord);
    };
} // namespace Js
