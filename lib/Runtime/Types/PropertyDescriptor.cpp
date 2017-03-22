//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    PropertyDescriptor::PropertyDescriptor() :
        writableSpecified(false),
        enumerableSpecified(false),
        configurableSpecified(false),
        valueSpecified(false),
        getterSpecified(false),
        setterSpecified(false),
        Writable(false),
        Enumerable(false),
        Configurable(false),
        Value(nullptr),
        Getter(nullptr),
        Setter(nullptr),
        originalVar(nullptr),
        fromProxy(false)
    {LOGMEIN("PropertyDescriptor.cpp] 23\n");
    }

    void PropertyDescriptor::SetEnumerable(bool value)
    {LOGMEIN("PropertyDescriptor.cpp] 27\n");
        Enumerable = value;
        enumerableSpecified = true;
    }

    void PropertyDescriptor::SetWritable(bool value)
    {LOGMEIN("PropertyDescriptor.cpp] 33\n");
        Writable = value;
        writableSpecified = true;
    }

    void PropertyDescriptor::SetConfigurable(bool value)
    {LOGMEIN("PropertyDescriptor.cpp] 39\n");
        Configurable = value;
        configurableSpecified = true;
    }

    void PropertyDescriptor::SetValue(Var value)
    {LOGMEIN("PropertyDescriptor.cpp] 45\n");
        this->Value = value;
        this->valueSpecified = true;
    }

    void PropertyDescriptor::SetGetter(Var getter)
    {LOGMEIN("PropertyDescriptor.cpp] 51\n");
        this->Getter = getter;
        this->getterSpecified = true;
    }

    void PropertyDescriptor::SetSetter(Var setter)
    {LOGMEIN("PropertyDescriptor.cpp] 57\n");
        this->Setter = setter;
        this->setterSpecified = true;
    }

    PropertyAttributes PropertyDescriptor::GetAttributes() const
    {LOGMEIN("PropertyDescriptor.cpp] 63\n");
        PropertyAttributes attributes = PropertyNone;

        if (this->configurableSpecified && this->Configurable)
        {LOGMEIN("PropertyDescriptor.cpp] 67\n");
            attributes |= PropertyConfigurable;
        }
        if (this->enumerableSpecified && this->Enumerable)
        {LOGMEIN("PropertyDescriptor.cpp] 71\n");
            attributes |= PropertyEnumerable;
        }
        if (this->writableSpecified && this->Writable)
        {LOGMEIN("PropertyDescriptor.cpp] 75\n");
            attributes |= PropertyWritable;
        }

        return attributes;
    }

    void PropertyDescriptor::SetAttributes(PropertyAttributes attributes, PropertyAttributes mask)
    {LOGMEIN("PropertyDescriptor.cpp] 83\n");
        if (mask & PropertyConfigurable)
        {LOGMEIN("PropertyDescriptor.cpp] 85\n");
            this->SetConfigurable(PropertyNone != (attributes & PropertyConfigurable));
        }
        if (mask & PropertyEnumerable)
        {LOGMEIN("PropertyDescriptor.cpp] 89\n");
            this->SetEnumerable(PropertyNone != (attributes & PropertyEnumerable));
        }
        if (mask & PropertyWritable)
        {LOGMEIN("PropertyDescriptor.cpp] 93\n");
            this->SetWritable(PropertyNone != (attributes & PropertyWritable));
        }
    }

    void PropertyDescriptor::MergeFrom(const PropertyDescriptor& descriptor)
    {LOGMEIN("PropertyDescriptor.cpp] 99\n");
        if (descriptor.configurableSpecified)
        {LOGMEIN("PropertyDescriptor.cpp] 101\n");
            this->SetConfigurable(descriptor.Configurable);
        }
        if (descriptor.enumerableSpecified)
        {LOGMEIN("PropertyDescriptor.cpp] 105\n");
            this->SetEnumerable(descriptor.Enumerable);
        }
        if (descriptor.writableSpecified)
        {LOGMEIN("PropertyDescriptor.cpp] 109\n");
            this->SetWritable(descriptor.Writable);
        }

        if (descriptor.valueSpecified)
        {LOGMEIN("PropertyDescriptor.cpp] 114\n");
            this->SetValue(descriptor.Value);
        }
        if (descriptor.getterSpecified)
        {LOGMEIN("PropertyDescriptor.cpp] 118\n");
            this->SetGetter(descriptor.Getter);
        }
        if (descriptor.setterSpecified)
        {LOGMEIN("PropertyDescriptor.cpp] 122\n");
            this->SetSetter(descriptor.Setter);
        }
    }
}
