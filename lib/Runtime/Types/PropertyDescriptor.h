//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    struct PropertyDescriptor
    {
    public:
        PropertyDescriptor();
        PropertyDescriptor(const PropertyDescriptor& other)
            :Value(other.Value),
            Getter(other.Getter),
            Setter(other.Setter),
            originalVar(other.originalVar),
            writableSpecified(other.writableSpecified),
            enumerableSpecified(other.enumerableSpecified),
            configurableSpecified(other.configurableSpecified),
            valueSpecified(other.valueSpecified),
            getterSpecified(other.getterSpecified),
            setterSpecified(other.setterSpecified),
            Writable(other.Writable),
            Enumerable(other.Enumerable),
            Configurable(other.Configurable),
            fromProxy(other.fromProxy)
        {LOGMEIN("PropertyDescriptor.h] 27\n");
        }

    private:
        Field(Var)  Value;
        Field(Var)  Getter;
        Field(Var)  Setter;
        Field(Var)  originalVar;

        Field(bool) writableSpecified;
        Field(bool) enumerableSpecified;
        Field(bool) configurableSpecified;
        Field(bool) valueSpecified;
        Field(bool) getterSpecified;
        Field(bool) setterSpecified;

        Field(bool) Writable;
        Field(bool) Enumerable;
        Field(bool) Configurable;
        Field(bool) fromProxy;

    public:
        bool IsDataDescriptor() const {LOGMEIN("PropertyDescriptor.h] 49\n"); return writableSpecified | valueSpecified;}
        bool IsAccessorDescriptor() const {LOGMEIN("PropertyDescriptor.h] 50\n"); return getterSpecified | setterSpecified;}
        bool IsGenericDescriptor() const {LOGMEIN("PropertyDescriptor.h] 51\n"); return !IsAccessorDescriptor() && !IsDataDescriptor(); }
        void SetEnumerable(bool value);
        void SetWritable(bool value);
        void SetConfigurable(bool value);

        void SetValue(Var value);
        Var GetValue() const {LOGMEIN("PropertyDescriptor.h] 57\n"); return Value; }

        void SetGetter(Var getter);
        Var GetGetter() const {LOGMEIN("PropertyDescriptor.h] 60\n"); Assert(getterSpecified || Getter == nullptr);  return Getter; }
        void SetSetter(Var setter);
        Var GetSetter() const {LOGMEIN("PropertyDescriptor.h] 62\n"); Assert(setterSpecified || Setter == nullptr); return Setter; }

        PropertyAttributes GetAttributes() const;

        bool IsFromProxy() const {LOGMEIN("PropertyDescriptor.h] 66\n"); return fromProxy; }
        void SetFromProxy(bool value) {LOGMEIN("PropertyDescriptor.h] 67\n"); fromProxy = value; }

        void SetOriginal(Var original) {LOGMEIN("PropertyDescriptor.h] 69\n"); originalVar = original; }
        Var GetOriginal() const {LOGMEIN("PropertyDescriptor.h] 70\n"); return originalVar; }

        bool ValueSpecified() const {LOGMEIN("PropertyDescriptor.h] 72\n"); return valueSpecified; }
        bool WritableSpecified() const {LOGMEIN("PropertyDescriptor.h] 73\n"); return writableSpecified; };
        bool ConfigurableSpecified() const {LOGMEIN("PropertyDescriptor.h] 74\n"); return configurableSpecified; }
        bool EnumerableSpecified() const {LOGMEIN("PropertyDescriptor.h] 75\n"); return enumerableSpecified; }
        bool GetterSpecified() const {LOGMEIN("PropertyDescriptor.h] 76\n"); return getterSpecified; }
        bool SetterSpecified() const {LOGMEIN("PropertyDescriptor.h] 77\n"); return setterSpecified; }

        bool IsWritable() const {LOGMEIN("PropertyDescriptor.h] 79\n"); Assert(writableSpecified);  return Writable; }
        bool IsEnumerable() const {LOGMEIN("PropertyDescriptor.h] 80\n"); Assert(enumerableSpecified); return Enumerable; }
        bool IsConfigurable() const {LOGMEIN("PropertyDescriptor.h] 81\n"); Assert(configurableSpecified);  return Configurable; }

        // Set configurable/enumerable/writable.
        // attributes: attribute values.
        // mask: specified which attributes to set. If an attribute is not in the mask.
        void SetAttributes(PropertyAttributes attributes, PropertyAttributes mask = ~PropertyNone);

        // Merge from descriptor parameter into this but only fields specified by descriptor parameter.
        void MergeFrom(const PropertyDescriptor& descriptor);
    };
}
