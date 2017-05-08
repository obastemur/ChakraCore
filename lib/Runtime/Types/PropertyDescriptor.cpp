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
    {TRACE_IT(66798);
    }

    void PropertyDescriptor::SetEnumerable(bool value)
    {TRACE_IT(66799);
        Enumerable = value;
        enumerableSpecified = true;
    }

    void PropertyDescriptor::SetWritable(bool value)
    {TRACE_IT(66800);
        Writable = value;
        writableSpecified = true;
    }

    void PropertyDescriptor::SetConfigurable(bool value)
    {TRACE_IT(66801);
        Configurable = value;
        configurableSpecified = true;
    }

    void PropertyDescriptor::SetValue(Var value)
    {TRACE_IT(66802);
        this->Value = value;
        this->valueSpecified = true;
    }

    void PropertyDescriptor::SetGetter(Var getter)
    {TRACE_IT(66803);
        this->Getter = getter;
        this->getterSpecified = true;
    }

    void PropertyDescriptor::SetSetter(Var setter)
    {TRACE_IT(66804);
        this->Setter = setter;
        this->setterSpecified = true;
    }

    PropertyAttributes PropertyDescriptor::GetAttributes() const
    {TRACE_IT(66805);
        PropertyAttributes attributes = PropertyNone;

        if (this->configurableSpecified && this->Configurable)
        {TRACE_IT(66806);
            attributes |= PropertyConfigurable;
        }
        if (this->enumerableSpecified && this->Enumerable)
        {TRACE_IT(66807);
            attributes |= PropertyEnumerable;
        }
        if (this->writableSpecified && this->Writable)
        {TRACE_IT(66808);
            attributes |= PropertyWritable;
        }

        return attributes;
    }

    void PropertyDescriptor::SetAttributes(PropertyAttributes attributes, PropertyAttributes mask)
    {TRACE_IT(66809);
        if (mask & PropertyConfigurable)
        {TRACE_IT(66810);
            this->SetConfigurable(PropertyNone != (attributes & PropertyConfigurable));
        }
        if (mask & PropertyEnumerable)
        {TRACE_IT(66811);
            this->SetEnumerable(PropertyNone != (attributes & PropertyEnumerable));
        }
        if (mask & PropertyWritable)
        {TRACE_IT(66812);
            this->SetWritable(PropertyNone != (attributes & PropertyWritable));
        }
    }

    void PropertyDescriptor::MergeFrom(const PropertyDescriptor& descriptor)
    {TRACE_IT(66813);
        if (descriptor.configurableSpecified)
        {TRACE_IT(66814);
            this->SetConfigurable(descriptor.Configurable);
        }
        if (descriptor.enumerableSpecified)
        {TRACE_IT(66815);
            this->SetEnumerable(descriptor.Enumerable);
        }
        if (descriptor.writableSpecified)
        {TRACE_IT(66816);
            this->SetWritable(descriptor.Writable);
        }

        if (descriptor.valueSpecified)
        {TRACE_IT(66817);
            this->SetValue(descriptor.Value);
        }
        if (descriptor.getterSpecified)
        {TRACE_IT(66818);
            this->SetGetter(descriptor.Getter);
        }
        if (descriptor.setterSpecified)
        {TRACE_IT(66819);
            this->SetSetter(descriptor.Setter);
        }
    }
}
