//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

#if DBG
#include "Common/Int32Math.h"
#include "DataStructures/List.h"
#include "Memory/StressTest.h"
#if !USING_PAL_STDLIB
#include <malloc.h>
#endif

typedef JsUtil::BaseDictionary<TestObject*, bool, RecyclerNonLeafAllocator> ObjectTracker_t;
typedef JsUtil::List<TestObject*, Recycler> ObjectList_t;

template<size_t align> bool IsAligned(void *p)
{TRACE_IT(27054);
    return (reinterpret_cast<size_t>(p) & (align - 1)) == 0;
}

TestObject::TestObject(size_t _size, int _pointerCount) : size(_size), pointerCount(_pointerCount)
{TRACE_IT(27055);
    cookie = CalculateCookie();
    memset(GetDataPointer(), 0, pointerCount * sizeof(TestObject*));
}

size_t TestObject::CalculateCookie()
{TRACE_IT(27056);
    return reinterpret_cast<size_t>(this) ^ (static_cast<size_t>(pointerCount) << 12) ^ (size << 24) + 1;
}

void TestObject::CheckCookie()
{TRACE_IT(27057);

    Assert((reinterpret_cast<size_t>(this)& (OBJALIGN - 1)) == 0);

    Assert(cookie == CalculateCookie());
}

TestObject *TestObject::Get(int index)
{TRACE_IT(27058);
    Assert(index < pointerCount);

    TestObject *addr = GetDataPointer()[index];

    Assert((reinterpret_cast<size_t>(addr) & (OBJALIGN - 1)) == 0);

    return addr;
}

void TestObject::Set(int index, TestObject *val)
{TRACE_IT(27059);
    Assert(index < pointerCount);

    GetDataPointer()[index] = val;
}

void TestObject::SetRandom(TestObject *val)
{TRACE_IT(27060);
    if (pointerCount != 0)
    {
        Set(rand() % pointerCount, val);
    }
}

void TestObject::Add(TestObject *val)
{TRACE_IT(27061);
    TestObject **data = GetDataPointer();

    for (int i = 0; i < pointerCount; ++i)
    {TRACE_IT(27062);
        if (data[i] == nullptr/* || !IsAligned<64>(data[i])*/)
        {TRACE_IT(27063);
            data[i] = val;
            break;
        }
    }
}

void TestObject::ClearOne()
{TRACE_IT(27064);
    CheckCookie();

    TestObject **data = GetDataPointer();

    for (int i = 0; i < pointerCount; ++i)
    {TRACE_IT(27065);
        if (data[i] != nullptr/* && IsAligned<64>(data[i])*/)
        {TRACE_IT(27066);
            // CreateFalseReferenceRandom(data[i]);
            data[i] = nullptr;
            break;
        }
    }
}

void TestObject::Visit(Recycler *recycler, TestObject *root)
{
    Visit(recycler, root, [](TestObject*) { });
}

template<class Fn> void TestObject::Visit(Recycler *recycler, TestObject *root, Fn fn)
{TRACE_IT(27067);
    // TODO: move these allocations to HeapAllocator.

    ObjectTracker_t *objectTracker = RecyclerNew(recycler, ObjectTracker_t, recycler);
    ObjectList_t *objectList = RecyclerNew(recycler, ObjectList_t, recycler);

    // Prime the list with the first object
    objectList->Add(root);
    objectTracker->Add(root, true);

    int numObjects = 0;
    while (objectList->Count() > 0)
    {TRACE_IT(27068);
        TestObject *curr = objectList->Item(0);
        objectList->RemoveAt(0);

        curr->CheckCookie();
        for (int i = 0; i < curr->pointerCount; ++i)
        {TRACE_IT(27069);
            TestObject *obj = curr->Get(i);
            if (obj != nullptr /*&& IsAligned<64>(obj)*/ && !objectTracker->ContainsKey(obj))
            {TRACE_IT(27070);
                objectTracker->Add(obj, true);
                objectList->Add(obj);
            }
        }

        ++numObjects;
    }

    objectTracker->Map([&](TestObject * val, bool) {
        fn(val);
    });

}

TestObject* TestObject::Create(Recycler *recycler, int pointerCount, size_t extraBytes, CreateOptions options)
{TRACE_IT(27071);
    size_t size = sizeof(TestObject)+pointerCount * sizeof(TestObject*) + extraBytes;

    if (options == NormalObj)
    {TRACE_IT(27072);
        return RecyclerNewPlus(recycler, size, TestObject, size, pointerCount);
    }
    else if (options == LeafObj)
    {TRACE_IT(27073);
        Assert(pointerCount == 0);
        return RecyclerNewPlusLeaf(recycler, size, TestObject, size, pointerCount);
    }
    else
    {TRACE_IT(27074);
        Assert(false);
        return nullptr;
    }
}

void TestObject::CreateFalseReferenceRandom(TestObject *val)
{TRACE_IT(27075);
    char *addr = reinterpret_cast<char*>(val);
    addr += 32;
    SetRandom(reinterpret_cast<TestObject*>(addr));
}

StressTester::StressTester(Recycler *_recycler) : recycler(_recycler)
{TRACE_IT(27076);
    uint seed = (uint)time(NULL);
    printf("Random seed: %u\n", seed);
    srand(seed);
}

size_t StressTester::GetRandomSize()
{TRACE_IT(27077);
    int i = rand() % 5;
    switch (i)
    {
    case 0: return 0;
    case 1: return rand() % 16;
    case 2: return rand() % 4096;
    case 3: return rand() % 16384;
    case 4: return rand();
    default:
        Assert(false);
        return 0;
    }
}

TestObject* StressTester::CreateLinkedList()
{TRACE_IT(27078);
    TestObject *root = TestObject::Create(recycler, 1, GetRandomSize());
    TestObject *curr = root;
    int length = rand() % MaxLinkedListLength;
    for (int i = 0; i < length; ++i)
    {TRACE_IT(27079);
        CreateOptions options = (i == length - 1) ? LeafObj : NormalObj;
        TestObject *next = TestObject::Create(recycler, options == LeafObj ? 0 : 1, GetRandomSize());
        curr->Add(next);
        curr = next;
    }
    return root;
}


void StressTester::CreateTreeHelper(TestObject *root, int depth) {TRACE_IT(27080);
    for (int i = 0; i < root->pointerCount; ++i, ++treeTotal)
    {TRACE_IT(27081);
        if (depth == 0 || treeTotal > MaxNodesInTree)
        {TRACE_IT(27082);
            root->Add(TestObject::Create(recycler, 0, rand(), LeafObj));
        }
        else
        {TRACE_IT(27083);
            TestObject *newObj = TestObject::Create(recycler, 4, GetRandomSize());
            CreateTreeHelper(newObj, depth - 1);
            root->Add(newObj);
        }
    }
};

TestObject* StressTester::CreateTree()
{TRACE_IT(27084);
    TestObject *root = TestObject::Create(recycler, 4, GetRandomSize());
    treeTotal = 0;
    CreateTreeHelper(root, rand() % MaxTreeDepth);
    return root;
}

TestObject *StressTester::CreateRandom()
{TRACE_IT(27085);
    int numObjects = rand() % 5000 + 1;

    void *memory = _alloca(numObjects * sizeof(TestObject*)+OBJALIGN);
    TestObject **objs = reinterpret_cast<TestObject**>(AlignPtr(memory, OBJALIGN));

    // Create the objects
    for (int i = 0; i < numObjects; ++i)
    {TRACE_IT(27086);
        objs[i] = TestObject::Create(recycler, 10, rand());
    }

    // Create links between objects
    for (int i = 0; i < numObjects; ++i)
    {TRACE_IT(27087);
        for (int j = 0; j < 5; ++j)
        {TRACE_IT(27088);
            objs[i]->SetRandom(objs[rand() % numObjects]);
        }
    }

    return objs[0];
}

void StressTester::Run()
{TRACE_IT(27089);

    const int stackExtraBytes = 1000;
    const int stackPointers = 50;
    const size_t sizeRequired = sizeof(TestObject)+stackExtraBytes + stackPointers * sizeof(TestObject*) + OBJALIGN;
    char memory[sizeRequired];
    void *addr = AlignPtr(memory, OBJALIGN);

    TestObject *stack = new (addr) TestObject(stackExtraBytes, stackPointers);

    auto ObjectVisitor = [&](TestObject *object) {TRACE_IT(27090);
        // Clear out one of the pointers.
        if (rand() % 5 == 0)
        {TRACE_IT(27091);
            object->ClearOne();
        }

        // Maybe store a pointer on the stack.
        if (rand() % 25 == 0)
        {TRACE_IT(27092);
            stack->SetRandom(object);
        }

        // Maybe add a stack reference to the current object
        if (rand() % 25 == 0)
        {TRACE_IT(27093);
            object->SetRandom(stack->Get(rand() % stack->pointerCount));
        }

    };

    while (1)
    {TRACE_IT(27094);
        TestObject *root = CreateLinkedList();
        TestObject::Visit(recycler, root);

        root = CreateTree();
        TestObject::Visit(recycler, root, ObjectVisitor);
        TestObject::Visit(recycler, root);

        root = CreateRandom();
        TestObject::Visit(recycler, root, ObjectVisitor);
        TestObject::Visit(recycler, root);

        TestObject::Visit(recycler, stack, [&](TestObject *object) {
            if (rand() % 10 == 0)
            {TRACE_IT(27095);
                object->ClearOne();
            }
        });

        if (rand() % 3 == 0)
        {TRACE_IT(27096);
            for (int i = 0; i < stack->pointerCount; ++i)
            {TRACE_IT(27097);
                stack->Set(i, nullptr);
            }
        }
    }
}
#endif
