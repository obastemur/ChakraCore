//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

namespace UnifiedRegex
{
    // The VS2013 linker treats this as a redefinition of an already
    // defined constant and complains. So skip the declaration if we're compiling
    // with VS2013 or below.
#if !defined(_MSC_VER) || _MSC_VER >= 1900
    const int CharTrie::initCapacity;
#endif

    // ----------------------------------------------------------------------
    // CharTrie
    // ----------------------------------------------------------------------
    inline bool CharTrie::Find(Char c, int& outi)
    {TRACE_IT(29116);
        if (count == 0)
        {TRACE_IT(29117);
            outi = 0;
            return false;
        }
        int l = 0;
        int h = count - 1;
        while (true)
        {TRACE_IT(29118);
            int m = (l + h) / 2;
            if (children[m].c == c)
            {TRACE_IT(29119);
                outi = m;
                return true;
            }
            else if (CTU(children[m].c) < CTU(c))
            {TRACE_IT(29120);
                l = m + 1;
                if (l > h)
                {TRACE_IT(29121);
                    outi = l;
                    return false;
                }
            }
            else
            {TRACE_IT(29122);
                h = m - 1;
                if (h < l)
                {TRACE_IT(29123);
                    outi = m;
                    return false;
                }
            }
        }
        return false;
    }

    void CharTrie::FreeBody(ArenaAllocator* allocator)
    {TRACE_IT(29124);
        for (int i = 0; i < count; i++)
            children[i].node.FreeBody(allocator);
        if (capacity > 0)
            AdeleteArray(allocator, capacity, children);
#if DBG
        count = 0;
        capacity = 0;
        children = 0;
#endif
    }

    CharTrie* CharTrie::Add(ArenaAllocator* allocator, Char c)
    {TRACE_IT(29125);
        int i;
        if (!Find(c, i))
        {TRACE_IT(29126);
            if (capacity <= count)
            {TRACE_IT(29127);
                int newCapacity = max(capacity * 2, initCapacity);
                children = (CharTrieEntry*)allocator->Realloc(children, capacity * sizeof(CharTrieEntry), newCapacity * sizeof(CharTrieEntry));
                capacity = newCapacity;
            }

            for (int j = count; j > i; j--)
            {TRACE_IT(29128);
                children[j].c = children[j - 1].c;
                children[j].node = children[j - 1].node;
            }
            children[i].c = c;
            children[i].node.Reset();
            count++;
        }
        return &children[i].node;
    }

    bool CharTrie::IsDepthZero() const
    {TRACE_IT(29129);
        return isAccepting && count == 0;
    }

    bool CharTrie::IsDepthOne() const
    {TRACE_IT(29130);
        if (isAccepting)
            return 0;
        for (int i = 0; i < count; i++)
        {TRACE_IT(29131);
            if (!children[i].node.IsDepthZero())
                return false;
        }
        return true;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void CharTrie::Print(DebugWriter* w) const
    {TRACE_IT(29132);
        w->Indent();
        if (isAccepting)
            w->PrintEOL(_u("<accept>"));
        for (int i = 0; i < count; i++)
        {TRACE_IT(29133);
            w->PrintQuotedChar(children[i].c);
            w->EOL();
            children[i].node.Print(w);
        }
        w->Unindent();
    }
#endif

    // ----------------------------------------------------------------------
    // RuntimeCharTrie
    // ----------------------------------------------------------------------
    bool RuntimeCharTrie::Match
        (const Char* const input
            , const CharCount inputLength
            , CharCount& inputOffset
#if ENABLE_REGEX_CONFIG_OPTIONS
            , RegexStats* stats
#endif
            ) const
    {TRACE_IT(29134);
        const RuntimeCharTrie* curr = this;
        while (true)
        {TRACE_IT(29135);
            if (curr->count == 0)
                return true;
            if (inputOffset >= inputLength)
                return false;
#if ENABLE_REGEX_CONFIG_OPTIONS
            if (stats != 0)
                stats->numCompares++;
#endif

#if 0
            int l = 0;
            int h = curr->count - 1;
            while (true)
            {TRACE_IT(29136);
                if (l > h)
                    return false;
                int m = (l + h) / 2;
                if (curr->children[m].c == input[inputOffset])
                {TRACE_IT(29137);
                    inputOffset++;
                    curr = &curr->children[m].node;
                    break;
                }
                else if (CTU(curr->children[m].c) < CTU(input[inputOffset]))
                    l = m + 1;
                else
                    h = m - 1;
            }
#else
            int i = 0;
            while (true)
            {TRACE_IT(29138);
                if (curr->children[i].c == input[inputOffset])
                {TRACE_IT(29139);
                    inputOffset++;
                    curr = &curr->children[i].node;
                    break;
                }
                else if (curr->children[i].c > input[inputOffset])
                    return false;
                else if (++i >= curr->count)
                    return false;
            }
#endif
        }
    }

    void RuntimeCharTrie::FreeBody(ArenaAllocator* allocator)
    {TRACE_IT(29140);
        for (int i = 0; i < count; i++)
            children[i].node.FreeBody(allocator);
        if (count > 0)
            AdeleteArray(allocator, count, children);
#if DBG
        count = 0;
        children = 0;
#endif
    }

    void RuntimeCharTrie::CloneFrom(ArenaAllocator* allocator, const CharTrie& other)
    {TRACE_IT(29141);
        count = other.count;
        if (count > 0)
        {TRACE_IT(29142);
            children = AnewArray(allocator, RuntimeCharTrieEntry, count);
            for (int i = 0; i < count; i++)
            {TRACE_IT(29143);
                children[i].c = other.children[i].c;
                children[i].node.CloneFrom(allocator,  other.children[i].node);
            }
        }
        else
            children = 0;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void RuntimeCharTrie::Print(DebugWriter* w) const
    {TRACE_IT(29144);
        w->Indent();
        for (int i = 0; i < count; i++)
        {TRACE_IT(29145);
            w->PrintQuotedChar(children[i].c);
            w->EOL();
            children[i].node.Print(w);
        }
        w->Unindent();
    }
#endif

}
