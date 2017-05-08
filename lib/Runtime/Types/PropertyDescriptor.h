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
        {TRACE_IT(66820);
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
        bool IsDataDescriptor() const {TRACE_IT(66821); return writableSpecified | valueSpecified;}
        bool IsAccessorDescriptor() const {TRACE_IT(66822); return getterSpecified | setterSpecified;}
        bool IsGenericDescriptor() const {TRACE_IT(66823); return !IsAccessorDescriptor() && !IsDataDescriptor(); }
        void SetEnumerable(bool value);
        void SetWritable(bool value);
        void SetConfigurable(bool value);

        void SetValue(Var value);
        Var GetValue() const {TRACE_IT(66824); return Value; }

        void SetGetter(Var getter);
        Var GetGetter() const {TRACE_IT(66825); Assert(getterSpecified || Getter == nullptr);  return Getter; }
        void SetSetter(Var setter);
        Var GetSetter() const {TRACE_IT(66826); Assert(setterSpecified || Setter == nullptr); return Setter; }

        PropertyAttributes GetAttributes() const;

        bool IsFromProxy() const {TRACE_IT(66827); return fromProxy; }
        void SetFromProxy(bool value) {TRACE_IT(66828); fromProxy = value; }

        void SetOriginal(Var original) {TRACE_IT(66829); originalVar = original; }
        Var GetOriginal() const {TRACE_IT(66830); return originalVar; }

        bool ValueSpecified() const {TRACE_IT(66831); return valueSpecified; }
        bool WritableSpecified() const {TRACE_IT(66832); return writableSpecified; };
        bool ConfigurableSpecified() const {TRACE_IT(66833); return configurableSpecified; }
        bool EnumerableSpecified() const {TRACE_IT(66834); return enumerableSpecified; }
        bool GetterSpecified() const {TRACE_IT(66835); return getterSpecified; }
        bool SetterSpecified() const {TRACE_IT(66836); return setterSpecified; }

        bool IsWritable() const {TRACE_IT(66837); Assert(writableSpecified);  return Writable; }
        bool IsEnumerable() const {TRACE_IT(66838); Assert(enumerableSpecified); return Enumerable; }
        bool IsConfigurable() const {TRACE_IT(66839); Assert(configurableSpecified);  return Configurable; }

        // Set configurable/enumerable/writable.
        // attributes: attribute values.
        // mask: specified which attributes to set. If an attribute is not in the mask.
        void SetAttributes(PropertyAttributes attributes, PropertyAttributes mask = ~PropertyNone);

        // Merge from descriptor parameter into this but only fields specified by descriptor parameter.
        void MergeFrom(const PropertyDescriptor& descriptor);
    };
}
