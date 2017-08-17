//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeBasePch.h"

#ifdef BARREL_MEMORY_SHOW_DIAGNOSTICS
size_t barrelMemory_totalMem = 0;
size_t barrelMemory_newMember = 0;
size_t barrelMemory_collision = 0;
#endif

//class Wrapped
//{
//public:
//    size_t index;
//    const void*  data;
//
//    Wrapped(): index(-1), data(nullptr) { }
//    Wrapped(size_t i, void* d): index(i), data(d) {}
//};

template <int chunkSize>
class BarrelMemory
{
public:
    size_t chunkCount;
    size_t activeIndex;
    class Barrel
    {
    public:
        char* chunk;
        Barrel* next;

        Barrel(): next(nullptr)
        {
            chunk = new char[chunkSize];
            memset(chunk, 0, chunkSize);
        }
    };

    Barrel* first;
    Barrel* current;

    BarrelMemory(): chunkCount(1), activeIndex(0)
    {
        first = new Barrel();
        current = first;
    }

    template <class T>
    inline T* Allocate(size_t size)
    {
#ifdef BARREL_MEMORY_SHOW_DIAGNOSTICS
        barrelMemory_totalMem += size;
#endif
        // assert size < chunkSize
        if (activeIndex + size >= chunkSize)
        {
            // findout what to do with remaining memory
            chunkCount++;
            activeIndex = 0;
            current->next = new Barrel();
            current = current->next;
        }

        T* mem = (T*) ((current->chunk) + activeIndex);
        activeIndex += size;
        return mem;
    }

    ~BarrelMemory()
    {
        current = first;
        while (current != nullptr)
        {
            Barrel* tmp = current->next;
            delete current;
            current = tmp;
        }
    }
};

BarrelMemory<16384> memory;
const void* LinearStorage[32 * 1024];

template <int blockCount, int minBlock, int maxBlock>
class Barrel
{
public:
  size_t activeIndex;
  static const int subBlockItemCount = (maxBlock - minBlock) / blockCount;
  class Boxes {
  public:
      class Box
      {
      public:
          Boxes *boxes;
          Box(): boxes(nullptr) { }

          void Initialize()
          {
              boxes = memory.Allocate<Boxes>(sizeof(Boxes) * subBlockItemCount);
          }
          ~Box()
          {
              // TODO: free boxes in BarrelMemory?
              boxes = nullptr;
          }
      };

      class Stash
      {
      public:
          Box*        blocks;
          const void* data;
          Stash(): blocks(nullptr), data(nullptr) { }

          void Initialize()
          {
              blocks = memory.Allocate<Box>(sizeof(Box) * blockCount);
          }

          ~Stash()
          {
              // TODO: free blocks in BarrelMemory?
              blocks = nullptr;
          }
      };

      Boxes():stash(nullptr) { }
      void Initialize()
      {
          stash = memory.Allocate<Stash>(sizeof(Stash));
      }

      Stash *stash;
  };

  Boxes box;

  size_t Add (const char16 *str, const void* data, bool symbol, size_t len)
  {
      Boxes* temp = &box;
      for (size_t i = 0; i < len; i++)
      {
          char16 wbyte = str[i];
          
          unsigned char *bytes;
          unsigned char tmp = (unsigned char)wbyte;
          int length;
          
          if (wbyte == tmp)
          {
              length = 1;
              bytes = &tmp;
          }
          else
          {
              length = 2;
              bytes = (unsigned char*)&wbyte;
          }
          
          for (int j = 0; j < length; j++)
          {
              unsigned char byte = bytes[j] - minBlock;
              int location = byte / subBlockItemCount;
              if (!temp->stash)
              {
                  temp->Initialize();
                  temp->stash->Initialize();
                  temp->stash->blocks[location].Initialize();
              }
              else if (!temp->stash->blocks)
              {
                  temp->stash->Initialize();
                  temp->stash->blocks[location].Initialize();
              }
              else if (!temp->stash->blocks[location].boxes)
              {
                  temp->stash->blocks[location].Initialize();
              }
              int subLocation = byte - (location * subBlockItemCount);
              temp = &(temp->stash->blocks[location].boxes[subLocation]);
          }
      }

      if (!temp->stash)
      {
          temp->Initialize();
      }
#ifdef BARREL_MEMORY_SHOW_DIAGNOSTICS
      if (temp->stash->wrapped.index == -1) barrelMemory_newMember++; else barrelMemory_collision++;
#endif
      size_t currentIndex = activeIndex++;
      LinearStorage[currentIndex] = data;
      
      if (!symbol || temp->stash->data == nullptr) // do not support symbol search from char buffer.
          temp->stash->data = data;

      return currentIndex;
  }

  const void* Find (const char16* str, size_t len)
  {
      Boxes* temp = &box;
      for (size_t i = 0; i < len; i++)
      {
          char16 wbyte = str[i];
          
          unsigned char *bytes;
          unsigned char tmp = (unsigned char)wbyte;
          int length;
          
          if (wbyte == tmp)
          {
              length = 1;
              bytes = &tmp;
          }
          else
          {
              length = 2;
              bytes = (unsigned char*)&wbyte;
          }
          
          for (int j = 0; j < length; j++)
          {
              unsigned char byte = bytes[j] - minBlock;
              if (!temp->stash || !temp->stash->blocks) return nullptr;

              int location = byte / subBlockItemCount;
              if (!temp->stash->blocks[location].boxes) return nullptr;

              int subLocation = byte - (location * subBlockItemCount);
              temp = &(temp->stash->blocks[location].boxes[subLocation]);
          }
      }
      if (!temp->stash)
      {
          return nullptr;
      }
      return temp->stash->data;
  }

  const void* FindByIndex(size_t index)
  {
      if (index == -1 || index >= activeIndex) return nullptr;
      return LinearStorage[index];
  }

  Barrel(): activeIndex(0) { }

#ifdef BARREL_MEMORY_SHOW_DIAGNOSTICS
  static void PrintDiagnostics()
  {
      printf("Box: %lu Boxes: %lu Stash: %lu \n", sizeof(Barrel<8, 32, 128>::Boxes), sizeof(Barrel<8, 32, 128>::Boxes::Box), sizeof(Barrel<8, 32, 128>::Boxes::Stash));
      printf("BM Total Mem: %lu \n", barrelMemory_totalMem);
      printf("BM Total NewMember: %lu \n", barrelMemory_newMember);
      printf("BM Total Mem / NewMember: %lu \n", barrelMemory_totalMem / barrelMemory_newMember);
      printf("BM Total Repeating: %lu \n", barrelMemory_collision);
  }
#endif
};

Barrel<16, 0, 256> members;
namespace Js
{

PropertyManager::PropertyManager()
{
    this->propertyMap = HeapNew(PropertyMap, &HeapAllocator::Instance, TotalNumberOfBuiltInProperties + 700);
}

PropertyManager::~PropertyManager()
{
    if (this->propertyMap != nullptr)
    {
        HeapDelete(this->propertyMap);
    }
    this->propertyMap = nullptr;
}

inline bool IsDirectPropertyName(const char16 * propertyName, int propertyNameLength)
{
    return ((propertyNameLength == 1) && ((propertyName[0] & 0xFF80) == 0));
}

bool PropertyManager::TryGetValueAt(Js::PropertyId propertyId, const Js::PropertyRecord ** propertyRecord)
{
    int32 propertyIndex = propertyId - Js::PropertyIds::_none;
    // return propertyMap->TryGetValueAt(propertyIndex, propertyRecord);

    *propertyRecord = (Js::PropertyRecord*) members.FindByIndex(propertyIndex);
    // printf("TGV: %lu %d - ", propertyId, *propertyRecord != nullptr);
    return *propertyRecord != nullptr;
}

const Js::PropertyRecord * PropertyManager::LookupWithKey(const char16 * buffer, const int length)
{
    // printf("LWK: %lu - ", length);
    return (Js::PropertyRecord*) members.Find(buffer, length);
    // return propertyMap->LookupWithKey(Js::HashedCharacterBuffer<char16>(buffer, length));
}

void PropertyManager::Remove(const Js::PropertyRecord * propertyRecord)
{
    // propertyMap->Remove(propertyRecord);
}

Js::PropertyId PropertyManager::GetNextPropertyId()
{
    // printf("GNP: %lu - ", members.activeIndex + Js::PropertyIds::_none);
    // return this->propertyMap->GetNextIndex() + Js::PropertyIds::_none;
    return members.activeIndex + Js::PropertyIds::_none;
}

Js::PropertyId PropertyManager::GetMaxPropertyId()
{
    // printf("GMP: %lu - ", members.activeIndex + Js::InternalPropertyIds::Count);
    // return this->Count() + Js::InternalPropertyIds::Count;
    return members.activeIndex + Js::InternalPropertyIds::Count;
}

uint PropertyManager::Count()
{
    // printf("CNT: %lu - ", members.activeIndex);
    return members.activeIndex;
    // return propertyMap->Count();
}

uint PropertyManager::GetLastIndex()
{
    // printf("GLI: %lu - ", members.activeIndex);
    return members.activeIndex ? members.activeIndex - 1 : 0;
    // return propertyMap->GetLastIndex();
}

void PropertyManager::LockResize()
{
    // propertyMap->LockResize();
}

void PropertyManager::UnlockResize()
{
    // propertyMap->UnlockResize();
}

void PropertyManager::EnsureCapacity()
{
    // propertyMap->EnsureCapacity();
}

void PropertyManager::Add(const Js::PropertyRecord * propertyRecord)
{
    // printf("ADD: %p %lu - ", propertyRecord, members.activeIndex);
    members.Add(propertyRecord->GetBuffer(), propertyRecord, propertyRecord->IsSymbol(), propertyRecord->GetLength());
    // propertyMap->Add(propertyRecord);
}

} // namespace Js
