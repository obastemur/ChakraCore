//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonDataStructuresPch.h"
#include "DataStructures/CharacterBuffer.h"
#include "DataStructures/InternalString.h"

namespace Js
{
    InternalString::InternalString(const CHAR_T* content, charcount_t length, unsigned char offset):
        m_content(content),
        m_charLength(length),
        m_offset(offset)
    {
        AssertMsg(length < INT_MAX, "Length should be a valid string length");
    }

    InternalString::InternalString(const CHAR_T* content, _no_write_barrier_tag, charcount_t length, unsigned char offset) :
        m_content(NO_WRITE_BARRIER_TAG(content)),
        m_charLength(length),
        m_offset(offset)
    {
        AssertMsg(length < INT_MAX, "Length should be a valid string length");
    }

    // This will make a copy of the entire buffer
    InternalString *InternalString::New(ArenaAllocator* alloc, const CHAR_T* content, charcount_t length)
    {
        size_t bytelength = sizeof(CHAR_T) * length;
        DWORD* allocbuffer = (DWORD*)alloc->Alloc(sizeof(DWORD) + bytelength + sizeof(CHAR_T));
        allocbuffer[0] = (DWORD) bytelength;

        CHAR_T* buffer = (CHAR_T*)(allocbuffer+1);
        js_memcpy_s(buffer, bytelength, content, bytelength);
        buffer[length] = _u('\0');
        InternalString* newInstance = Anew(alloc, InternalString, buffer, length);
        return newInstance;
    }

    // This will make a copy of the entire buffer
    // Allocated using recycler memory
    InternalString *InternalString::New(Recycler* recycler, const CHAR_T* content, charcount_t length)
    {
        size_t bytelength = sizeof(CHAR_T) * length;

        // Allocate 3 extra bytes, two for the first DWORD with the size, the third for the null character
        // This is so that we can pretend that internal strings are BSTRs for purposes of clients who want to use
        // it as thus
        const unsigned char offset = sizeof(DWORD)/sizeof(CHAR_T);
        InternalString* newInstance = RecyclerNewPlusLeaf(recycler, bytelength + (sizeof(DWORD) + sizeof(CHAR_T)), InternalString, nullptr, length, offset);
        DWORD* allocbuffer = (DWORD*) (newInstance + 1);
        allocbuffer[0] = (DWORD) bytelength;
        CHAR_T* buffer = (CHAR_T*)(allocbuffer + 1);
        js_memcpy_s(buffer, bytelength, content, bytelength);
        buffer[length] = _u('\0');
        newInstance->m_content = (const CHAR_T*) allocbuffer;

        return newInstance;
    }


    // This will only store the pointer and length, not making a copy of the buffer
    InternalString *InternalString::NewNoCopy(ArenaAllocator* alloc, const CHAR_T* content, charcount_t length)
    {
        InternalString* newInstance = Anew(alloc, InternalString, const_cast<CHAR_T*> (content), length);
        return newInstance;
    }
}
