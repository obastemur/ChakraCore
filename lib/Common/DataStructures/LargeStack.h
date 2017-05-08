//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
template <class T>
struct LargeStackBlock {
    T* items;
    int index;
    int itemCount;

    static LargeStackBlock<T>* Make(ArenaAllocator* alloc,int itemCount) {TRACE_IT(21747);
        LargeStackBlock<T>* block = AnewStruct(alloc, LargeStackBlock<T>);
        block->itemCount=itemCount;
        block->items = AnewArray(alloc, T, itemCount);
        block->index=0;
        return block;
    }

    BOOL Full() {TRACE_IT(21748); return index>=itemCount; }
    BOOL Empty() {TRACE_IT(21749); return index==0; }

    void Push(T item) {TRACE_IT(21750);
        AssertMsg(!Full(),"can't push to full stack block");
        items[index++]=item;
    }

    T Pop() {TRACE_IT(21751);
        AssertMsg(!Empty(),"can't pop empty stack block");
        index--;
        return items[index];
    }
};



template <class T>
class LargeStack {
    SList<LargeStackBlock<T>*>* blockStack;
    static const int BlockSize=8;
    static const int GrowSize=128;
    ArenaAllocator* alloc;

    LargeStack(ArenaAllocator* alloc) : alloc(alloc) {
        blockStack=Anew(alloc,SList<LargeStackBlock<T>*>,alloc);
        blockStack->Push(LargeStackBlock<T>::Make(alloc,BlockSize));
    }
public:
    static LargeStack * New(ArenaAllocator* alloc)
    {TRACE_IT(21752);
        return Anew(alloc, LargeStack, alloc);
    }

    void Push(T item) {TRACE_IT(21753);
        LargeStackBlock<T>* top=blockStack->Top();
        if (top->Full()) {TRACE_IT(21754);
            top=LargeStackBlock<T>::Make(alloc,top->itemCount+GrowSize);
            blockStack->Push(top);
        }
        top->Push(item);
    }

    BOOL Empty() {TRACE_IT(21755);
        LargeStackBlock<T>* top=blockStack->Top();
        if (top->Empty()) {TRACE_IT(21756);
            if (blockStack->HasOne()) {TRACE_IT(21757);
                // Avoid popping the last empty block to reduce freelist overhead.
                return true;
            }

            blockStack->Pop();
            return blockStack->Empty();
        }
        else return false;
    }

    T Pop() {TRACE_IT(21758);
        LargeStackBlock<T>* top=blockStack->Top();
        if (top->Empty()) {TRACE_IT(21759);
            blockStack->Pop();
            AssertMsg(!blockStack->Empty(),"can't pop empty block stack");
            top=blockStack->Top();
        }
        return top->Pop();
    }
};
