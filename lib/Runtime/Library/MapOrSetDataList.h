//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// This is a special use doubly linked list whose iterators are always valid
// no matter what modifications are made to the list during iteration. The
// iterators rely on deleted nodes still having valid next and prev references
// to the nodes they used to be next to before being removed. They also rely
// on nodes being recycler allocated so that active iterators do not need to
// be tracked by the list and updated when deletes occur. Finally, well
// defined iteration order is maintained by only allowing new items to be
// appended to the end of the list. This allows an iterator to validate
// itself by seeking backwards until it finds a node whose previous's next
// still points to it.
//
// The intended use of this list is to track insertion order for items added
// to ES6 Map and Set objects. If a more general use if found for this data
// structure please generalize it and consider moving it to Common\DataStructures.

namespace Js
{
    template <typename TData>
    class MapOrSetDataNode
    {
    private:
        template <typename TData>
        friend class MapOrSetDataList;

        Field(MapOrSetDataNode<TData>*) next;
        Field(MapOrSetDataNode<TData>*) prev;

        MapOrSetDataNode(TData& data) : data(data), next(nullptr), prev(nullptr) {TRACE_IT(62316); }

    public:
        Field(TData) data;
    };

    template <typename TData>
    class MapOrSetDataList
    {
    private:
        Field(MapOrSetDataNode<TData>*) first;
        Field(MapOrSetDataNode<TData>*) last;

    public:
        MapOrSetDataList(VirtualTableInfoCtorEnum) {};
        MapOrSetDataList() : first(nullptr), last(nullptr) {TRACE_IT(62317); }

        class Iterator
        {
            Field(MapOrSetDataList<TData>*) list;
            Field(MapOrSetDataNode<TData>*) current;
        public:
            Iterator() : list(nullptr), current(nullptr) {TRACE_IT(62318); }
            Iterator(MapOrSetDataList<TData>* list) : list(list), current(nullptr) {TRACE_IT(62319); }

            bool Next()
            {TRACE_IT(62320);
                // Nodes can be deleted while iterating so validate current
                // and if it is not valid find last valid node by following
                // previous toward first.
                // Note: clear will simply null out first and last, but
                // not invalidated nodes, so we must also check to see that
                // first is not null

                if (list == nullptr || list->first == nullptr)
                {TRACE_IT(62321);
                    // list is empty or was cleared during enumeration
                    list = nullptr;
                    current = nullptr;
                    return false;
                }

                if (current)
                {TRACE_IT(62322);
                    while (current->prev && current->prev->next != current)
                    {TRACE_IT(62323);
                        current = current->prev;
                    }

                    if (current->prev == nullptr && current != list->first)
                    {TRACE_IT(62324);
                        current = list->first;

                        if (current != nullptr)
                        {TRACE_IT(62325);
                            return true;
                        }

                        list = nullptr;
                        current = nullptr;
                        return false;
                    }

                    if (current->next == nullptr && current != list->last)
                    {TRACE_IT(62326);
                        Assert(list->last == nullptr);
                        current = nullptr;
                    }
                }

                if (current != list->last)
                {TRACE_IT(62327);
                    if (current == nullptr)
                    {TRACE_IT(62328);
                        Assert(list->first != nullptr);
                        current = list->first;
                    }
                    else
                    {TRACE_IT(62329);
                        current = current->next;
                    }
                    return true;
                }

                list = nullptr;
                current = nullptr;
                return false;
            }

            const TData& Current() const
            {TRACE_IT(62330);
                return current->data;
            }
        };

        void Clear()
        {TRACE_IT(62331);
            first = nullptr;
            last = nullptr;
        }

        MapOrSetDataNode<TData>* Append(TData& data, Recycler* recycler)
        {TRACE_IT(62332);
            // Must allocate with the recycler so that iterators can continue
            // to point to removed nodes.  That is, cannot delete nodes when
            // they are removed.  Reference counting would also work.
            MapOrSetDataNode<TData>* newNode = RecyclerNew(recycler, MapOrSetDataNode<TData>, data);

            if (last == nullptr)
            {TRACE_IT(62333);
                Assert(first == nullptr);
                first = newNode;
                last = newNode;
            }
            else
            {TRACE_IT(62334);
                newNode->prev = last;
                last->next = newNode;
                last = newNode;
            }

            return newNode;
        }

        void Remove(MapOrSetDataNode<TData>* node)
        {TRACE_IT(62335);
            // Cannot delete the node itself, nor change its next and prev pointers!
            // Otherwise active iterators may break. Iterators depend on nodes existing
            // until garbage collector picks them up.
            auto next = node->next;
            auto prev = node->prev;

            if (next)
            {TRACE_IT(62336);
                next->prev = prev;
            }
            else
            {TRACE_IT(62337);
                Assert(last == node);
                last = prev;
            }

            if (prev)
            {TRACE_IT(62338);
                prev->next = next;
            }
            else
            {TRACE_IT(62339);
                Assert(first == node);
                first = next;
            }
        }

        Iterator GetIterator()
        {TRACE_IT(62340);
            return Iterator(this);
        }
    };
}
