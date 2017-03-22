//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    //
    // An enumerator to enumerate JavascriptArray index property names as uint32 indices.
    //
    class JavascriptArrayIndexStaticEnumerator
    {
    public:
        typedef JavascriptArray ArrayType;

    private:
        JavascriptArray* m_array;       // The JavascriptArray to enumerate on
        uint32 m_index;                 // Current index

    public:
        JavascriptArrayIndexStaticEnumerator(JavascriptArray* array)
            : m_array(array)
        {LOGMEIN("JavascriptArrayIndexStaticEnumerator.h] 23\n");
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(m_array);
#endif
            Reset();
        }

        //
        // Reset to enumerate from beginning.
        //
        void Reset()
        {LOGMEIN("JavascriptArrayIndexStaticEnumerator.h] 34\n");
            m_index = JavascriptArray::InvalidIndex;
        }

        //
        // Get the current index. Valid only when MoveNext() returns true.
        //
        uint32 GetIndex() const
        {LOGMEIN("JavascriptArrayIndexStaticEnumerator.h] 42\n");
            return m_index;
        }

        //
        // Move to next index. If successful, use GetIndex() to get the index.
        //
        bool MoveNext()
        {LOGMEIN("JavascriptArrayIndexStaticEnumerator.h] 50\n");
            m_index = m_array->GetNextIndex(m_index);

            if (m_index != JavascriptArray::InvalidIndex)
            {LOGMEIN("JavascriptArrayIndexStaticEnumerator.h] 54\n");
                return true;
            }

            return false;
        }
    };
}
