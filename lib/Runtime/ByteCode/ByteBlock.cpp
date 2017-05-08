//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

namespace Js
{
    uint ByteBlock::GetLength() const
    {TRACE_IT(38352);
        return m_contentSize;
    }

    const byte* ByteBlock::GetBuffer() const
    {TRACE_IT(38353);
        return m_content;
    }

    byte* ByteBlock::GetBuffer()
    {TRACE_IT(38354);
        return m_content;
    }

    const byte ByteBlock::operator[](uint itemIndex) const
    {TRACE_IT(38355);
        AssertMsg(itemIndex < m_contentSize, "Ensure valid offset");

        return m_content[itemIndex];
    }

    byte& ByteBlock::operator[] (uint itemIndex)
    {TRACE_IT(38356);
        AssertMsg(itemIndex < m_contentSize, "Ensure valid offset");

        return m_content[itemIndex];
    }

    ByteBlock *ByteBlock::New(Recycler *alloc, const byte * initialContent, int initialContentSize)
    {TRACE_IT(38357);
        // initialContent may be 'null' if no data to copy
        AssertMsg(initialContentSize > 0, "Must have valid data size");

        ByteBlock *newBlock = RecyclerNew(alloc, ByteBlock, initialContentSize, alloc);

        //
        // Copy any optional data into the block:
        // - If initialContent was not provided, the block's contents will be uninitialized.
        //
        if (initialContent != nullptr)
        {TRACE_IT(38358);
            js_memcpy_s(newBlock->m_content, newBlock->GetLength(), initialContent, initialContentSize);
        }

        return newBlock;
    }

    ByteBlock *ByteBlock::NewFromArena(ArenaAllocator *alloc, const byte * initialContent, int initialContentSize)
    {TRACE_IT(38359);
        // initialContent may be 'null' if no data to copy
        AssertMsg(initialContentSize > 0, "Must have valid data size");

        ByteBlock *newBlock = Anew(alloc, ByteBlock, initialContentSize, alloc);

        //
        // Copy any optional data into the block:
        // - If initialContent was not provided, the block's contents will be uninitialized.
        //
        if (initialContent != nullptr)
        {TRACE_IT(38360);
            js_memcpy_s(newBlock->m_content, newBlock->GetLength(), initialContent, initialContentSize);
        }

        return newBlock;
    }

    ByteBlock *ByteBlock::New(Recycler *alloc, const byte * initialContent, int initialContentSize, ScriptContext * requestContext)
    {TRACE_IT(38361);
        // initialContent may be 'null' if no data to copy
        AssertMsg(initialContentSize > 0, "Must have valid data size");

        ByteBlock *newBlock = RecyclerNew(alloc, ByteBlock, initialContentSize, alloc);

        //
        // Copy any optional data into the block:
        // - If initialContent was not provided, the block's contents will be uninitialized.
        //
        if (initialContent != nullptr)
        {TRACE_IT(38362);
            //
            // Treat initialContent as array of vars
            // Clone vars to the requestContext
            //

            Var *src = (Var*)initialContent;
            Var *dst = (Var*)(byte*)newBlock->m_content;
            size_t count = initialContentSize / sizeof(Var);

            for (size_t i = 0; i < count; i++)
            {TRACE_IT(38363);
                if (TaggedInt::Is(src[i]))
                {TRACE_IT(38364);
                    dst[i] = src[i];
                }
                else
                {TRACE_IT(38365);
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
