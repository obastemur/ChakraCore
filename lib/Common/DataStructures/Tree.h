//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


///----------------------------------------------------------------------------
///----------------------------------------------------------------------------
///
/// class TreeNode
///
/// General DataStructure for an N-ary tree.
///
///----------------------------------------------------------------------------
///----------------------------------------------------------------------------

template<class T, int N>
class TreeNode
{

// Data
private:
    T                   value;
    TreeNode *          children[N];
    TreeNode<T, N> *    parent;

// Constructor
public:
    TreeNode(TreeNode<T, N> * parent = NULL)
    {TRACE_IT(22436);
        this->parent    = parent;
        for(int i = 0; i < N; i++)
        {TRACE_IT(22437);
            this->children[i] = NULL;
        }
    }

// Methods
public:
    bool ChildExistsAt(int i)
    {TRACE_IT(22438);
        return NULL != this->children[i];
    }

    TreeNode<T, N> * GetChildAt(int i)
    {TRACE_IT(22439);
        return this->children[i];
    }

    void SetChildAt(int i, TreeNode<T, N> *node)
    {TRACE_IT(22440);
        this->children[i]   = node;
    }

    TreeNode<T, N> * GetParent()
    {TRACE_IT(22441);
         return this->parent;
    }

    void SetParent(TreeNode<T, N>* parent)
    {TRACE_IT(22442);
        this->parent = parent;
    }

    T * GetValue()
    {TRACE_IT(22443);
        return &this->value;
    }

    void SetValue(const T value)
    {TRACE_IT(22444);
        this->value = value;
    }

};
