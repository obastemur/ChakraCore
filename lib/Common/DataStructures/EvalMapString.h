//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template <bool fastHash>
    struct EvalMapStringInternal
    {
        JsUtil::CharacterBuffer<char16> str;
        hash_t hash;
        ModuleID moduleID;
        BOOL strict;
        BOOL isLibraryCode;

        EvalMapStringInternal() : str(), moduleID(0), strict(FALSE), isLibraryCode(FALSE), hash(0) {TRACE_IT(21305);};
        EvalMapStringInternal(__in_ecount(charLength) char16 const* content, int charLength, ModuleID moduleID, BOOL strict, BOOL isLibraryCode)
            : str(content, charLength), moduleID(moduleID), strict(strict), isLibraryCode(isLibraryCode)
        {TRACE_IT(21306);
            // NOTE: this hash is not equivalent to the character buffer hash
            // Don't use a CharacteBuffer to do a map lookup on the EvalMapString.
            if (fastHash)
            {TRACE_IT(21307);
                hash = TAGHASH(str.FastHash());
            }
            else
            {TRACE_IT(21308);
                hash = TAGHASH((hash_t)str);
            }
        };

        EvalMapStringInternal& operator=(void * str)
        {TRACE_IT(21309);
            Assert(str == null);
            memset(this, 0, sizeof(EvalMapString));
            return (*this);
        }

        inline ModuleID GetModuleID() const
        {TRACE_IT(21310);
            return moduleID;
        }

        inline BOOL IsStrict() const
        {TRACE_IT(21311);
            return strict;
        }

        // Equality and hash function
        bool operator==(EvalMapStringInternal const& other) const
        {TRACE_IT(21312);
             return this->str == other.str &&
                this->GetModuleID() == other.GetModuleID() &&
                this->IsStrict() == other.IsStrict() &&
                this->isLibraryCode == other.isLibraryCode;
        }

        operator hash_t() const
        {TRACE_IT(21313);
            return UNTAGHASH(hash);
        }
    };
    typedef EvalMapStringInternal<true> FastEvalMapString;
    typedef EvalMapStringInternal<false> EvalMapString;

    void ConvertKey(const FastEvalMapString& src, EvalMapString& dest);
};
