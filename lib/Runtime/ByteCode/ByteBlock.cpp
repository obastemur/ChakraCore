//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

namespace Js
{
    uint ByteBlock::GetLength() const
    {LOGMEIN("ByteBlock.cpp] 9\n");
        return m_contentSize;
    }

    const byte* ByteBlock::GetBuffer() const
    {LOGMEIN("ByteBlock.cpp] 14\n");
        return m_content;
    }

    byte* ByteBlock::GetBuffer()
    {LOGMEIN("ByteBlock.cpp] 19\n");
        return m_content;
    }

    const byte ByteBlock::operator[](uint itemIndex) const
    {LOGMEIN("ByteBlock.cpp] 24\n");
        AssertMsg(itemIndex < m_contentSize, "Ensure valid offset");

        return m_content[itemIndex];
    }

    byte& ByteBlock::operator[] (uint itemIndex)
    {LOGMEIN("ByteBlock.cpp] 31\n");
        AssertMsg(itemIndex < m_contentSize, "Ensure valid offset");

        return m_content[itemIndex];
    }

    ByteBlock *ByteBlock::New(Recycler *alloc, const byte * initialContent, int initialContentSize)
    {LOGMEIN("ByteBlock.cpp] 38\n");
        // initialContent may be 'null' if no data to copy
        AssertMsg(initialContentSize > 0, "Must have valid data size");

        ByteBlock *newBlock = RecyclerNew(alloc, ByteBlock, initialContentSize, alloc);

        //
        // Copy any optional data into the block:
        // - If initialContent was not provided, the block's contents will be uninitialized.
        //
        if (initialContent != nullptr)
        {LOGMEIN("ByteBlock.cpp] 49\n");
            js_memcpy_s(newBlock->m_content, newBlock->GetLength(), initialContent, initialContentSize);
        }

        return newBlock;
    }

    ByteBlock *ByteBlock::NewFromArena(ArenaAllocator *alloc, const byte * initialContent, int initialContentSize)
    {LOGMEIN("ByteBlock.cpp] 57\n");
        // initialContent may be 'null' if no data to copy
        AssertMsg(initialContentSize > 0, "Must have valid data size");

        ByteBlock *newBlock = Anew(alloc, ByteBlock, initialContentSize, alloc);

        //
        // Copy any optional data into the block:
        // - If initialContent was not provided, the block's contents will be uninitialized.
        //
        if (initialContent != nullptr)
        {LOGMEIN("ByteBlock.cpp] 68\n");
            js_memcpy_s(newBlock->m_content, newBlock->GetLength(), initialContent, initialContentSize);
        }

        return newBlock;
    }

    ByteBlock *ByteBlock::New(Recycler *alloc, const byte * initialContent, int initialContentSize, ScriptContext * requestContext)
    {LOGMEIN("ByteBlock.cpp] 76\n");
        // initialContent may be 'null' if no data to copy
        AssertMsg(initialContentSize > 0, "Must have valid data size");

        ByteBlock *newBlock = RecyclerNew(alloc, ByteBlock, initialContentSize, alloc);

        //
        // Copy any optional data into the block:
        // - If initialContent was not provided, the block's contents will be uninitialized.
        //
        if (initialContent != nullptr)
        {LOGMEIN("ByteBlock.cpp] 87\n");
            //
            // Treat initialContent as array of vars
            // Clone vars to the requestContext
            //

            Var *src = (Var*)initialContent;
            Var *dst = (Var*)(byte*)newBlock->m_content;
            size_t count = initialContentSize / sizeof(Var);

            for (size_t i = 0; i < count; i++)
            {LOGMEIN("ByteBlock.cpp] 98\n");
                if (TaggedInt::Is(src[i]))
                {LOGMEIN("ByteBlock.cpp] 100\n");
                    dst[i] = src[i];
                }
                else
                {
                    //
                    // Currently only numbers are put into AuxiliaryContext data
                    //
                    Assert(JavascriptNumber::Is(src[i]));
                    dst[i] = JavascriptNumber::CloneToScriptContext(src[i], requestContext);
                    requestContext->BindReference(dst[i]);
                }
            }
        }

        return newBlock;
    }
}
