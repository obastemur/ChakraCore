//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {

    // Cache property index and IsWritable info for UpdatePatch
    class PropertyValueInfo
    {
        enum CacheInfoFlag
        {
            preventFalseReferenceFlag = 0x1, // avoid false positive for GC
            disablePrototypeCacheFlag = 0x2,
            enableStoreFieldCacheFlag = 0x4,
            defaultInfoFlags = preventFalseReferenceFlag | enableStoreFieldCacheFlag
        };

    private:
        RecyclableObject* m_instance;    // Slot owner instance
        PropertyIndex m_propertyIndex;   // Slot index on m_instance for the property, or NoSlot to indicate the object can't cache
        PropertyAttributes m_attributes; // Attributes of the property -- only Writable is used
        InlineCacheFlags flags;
        CacheInfoFlag cacheInfoFlag;
        InlineCache* inlineCache;
        PolymorphicInlineCache* polymorphicInlineCache;
        FunctionBody * functionBody;
        uint inlineCacheIndex;
        bool allowResizingPolymorphicInlineCache;

        void Set(RecyclableObject* instance, PropertyIndex propertyIndex, PropertyAttributes attributes, InlineCacheFlags flags)
        {TRACE_IT(66927);
            m_instance = instance;
            m_propertyIndex = propertyIndex;
            m_attributes = attributes;
            this->flags = flags;
        }

        void SetInfoFlag(CacheInfoFlag newFlag)  {TRACE_IT(66928); cacheInfoFlag = (CacheInfoFlag)(cacheInfoFlag | newFlag); }
        void ClearInfoFlag(CacheInfoFlag newFlag)  {TRACE_IT(66929); cacheInfoFlag = (CacheInfoFlag)(cacheInfoFlag & ~newFlag); }
        BOOL IsInfoFlagSet(CacheInfoFlag checkFlag) const {TRACE_IT(66930); return (cacheInfoFlag & checkFlag) == checkFlag; }

    public:
        PropertyValueInfo()
            : m_instance(NULL), m_propertyIndex(Constants::NoSlot), m_attributes(PropertyNone), flags(InlineCacheNoFlags),
            cacheInfoFlag(CacheInfoFlag::defaultInfoFlags), inlineCache(NULL), polymorphicInlineCache(NULL), functionBody(NULL),
            inlineCacheIndex(Constants::NoInlineCacheIndex),
            allowResizingPolymorphicInlineCache(true)
        {TRACE_IT(66931);
        }

        RecyclableObject* GetInstance() const       {TRACE_IT(66932); return m_instance; }
        PropertyIndex GetPropertyIndex() const      {TRACE_IT(66933); return m_propertyIndex; }
        bool IsWritable() const                     {TRACE_IT(66934); return (m_attributes & PropertyWritable) != 0; }
        bool IsEnumerable() const                   {TRACE_IT(66935); return (m_attributes & PropertyEnumerable) != 0; }
        bool IsNoCache() const                      {TRACE_IT(66936); return m_instance && m_propertyIndex == Constants::NoSlot; }
        void AddFlags(InlineCacheFlags newFlag)     {TRACE_IT(66937); flags = (InlineCacheFlags)(flags | newFlag); }
        InlineCacheFlags GetFlags() const           {TRACE_IT(66938); return flags; }
        PropertyAttributes GetAttributes() const    {TRACE_IT(66939); return m_attributes; }

        // Set property index and IsWritable cache info
        static void Set(PropertyValueInfo* info, RecyclableObject* instance, PropertyIndex propertyIndex, PropertyAttributes attributes = PropertyWritable,
            InlineCacheFlags flags = InlineCacheNoFlags)
        {TRACE_IT(66940);
            if (info)
            {TRACE_IT(66941);
                info->Set(instance, propertyIndex, attributes, flags);
            }
        }

        static void SetCacheInfo(PropertyValueInfo* info, InlineCache *const inlineCache);
        static void SetCacheInfo(PropertyValueInfo* info, FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, const bool allowResizingPolymorphicInlineCache);
        static void SetCacheInfo(PropertyValueInfo* info, FunctionBody *const functionBody, PolymorphicInlineCache *const polymorphicInlineCache, const InlineCacheIndex inlineCacheIndex, const bool allowResizingPolymorphicInlineCache);
        static void ClearCacheInfo(PropertyValueInfo* info);

        inline InlineCache * GetInlineCache() const
        {TRACE_IT(66942);
            return this->inlineCache;
        }

        inline PolymorphicInlineCache * GetPolymorphicInlineCache() const
        {TRACE_IT(66943);
            return this->polymorphicInlineCache;
        }

        inline FunctionBody * GetFunctionBody() const
        {TRACE_IT(66944);
            return this->functionBody;
        }

        inline uint GetInlineCacheIndex() const
        {TRACE_IT(66945);
            return this->inlineCacheIndex;
        }

        bool AllowResizingPolymorphicInlineCache() const
        {TRACE_IT(66946);
            return allowResizingPolymorphicInlineCache;
        }

        // Set to indicate the instance can't cache property index / IsWritable
        static void SetNoCache(PropertyValueInfo* info, RecyclableObject* instance)
        {
            Set(info, instance, Constants::NoSlot, PropertyNone, InlineCacheNoFlags);
        }

        static void DisablePrototypeCache(PropertyValueInfo* info, RecyclableObject* instance)
        {TRACE_IT(66947);
            if (info)
            {TRACE_IT(66948);
                info->SetInfoFlag(disablePrototypeCacheFlag);
            }
        }

        static bool PrototypeCacheDisabled(const PropertyValueInfo* info)
        {TRACE_IT(66949);
            return (info != NULL) && !!info->IsInfoFlagSet(disablePrototypeCacheFlag);
        }

        static void DisableStoreFieldCache(PropertyValueInfo* info)
        {TRACE_IT(66950);
            if (info)
            {TRACE_IT(66951);
                info->ClearInfoFlag(enableStoreFieldCacheFlag);
            }
        }

        static bool IsStoreFieldCacheEnabled(const PropertyValueInfo* info)
        {TRACE_IT(66952);
            return (info != NULL) && !!info->IsInfoFlagSet(enableStoreFieldCacheFlag);
        }

        bool IsStoreFieldCacheEnabled() const
        {TRACE_IT(66953);
            return IsStoreFieldCacheEnabled(this);
        }

    };

    enum SideEffects : byte
    {
       SideEffects_None     = 0,
       SideEffects_MathFunc = 0x1,
       SideEffects_ValueOf  = 0x2,
       SideEffects_ToString = 0x4,
       SideEffects_Accessor = 0x8,

       SideEffects_ToPrimitive = SideEffects_ValueOf | SideEffects_ToString,
       SideEffects_Any      = SideEffects_MathFunc | SideEffects_ValueOf | SideEffects_ToString | SideEffects_Accessor
    };

    // int32 is used in JIT code to pass the flag
    // Used to tweak type system methods behavior.
    // Normally, use: PropertyOperation_None.
    enum PropertyOperationFlags : int32
    {
        PropertyOperation_None                          = 0x00,
        PropertyOperation_StrictMode                    = 0x01,
        PropertyOperation_Root                          = 0x02,  // Operation doesn't specify base

        // In particular, used by SetProperty/WithAttributes to throw, rather than return false, when then instance object is not extensible.
        PropertyOperation_ThrowIfNotExtensible          = 0x04,

        // Intent: avoid any checks and force the operation.
        // In particular, used by SetProperty/WithAttributes to force adding a property when an object is not extensible.
        PropertyOperation_Force                         = 0x08,

        // Initializing a property with a special internal value, which the user's code will never see.
        PropertyOperation_SpecialValue                  = 0x10,

        // Pre-initializing a property value before the user's code actually does.
        PropertyOperation_PreInit                       = 0x20,

        // Don't mark this fields as fixed in the type handler.
        PropertyOperation_NonFixedValue                 = 0x40,

        PropertyOperation_PreInitSpecialValue           = PropertyOperation_PreInit | PropertyOperation_SpecialValue,

        PropertyOperation_StrictModeRoot                = PropertyOperation_StrictMode | PropertyOperation_Root,

        // No need to check for undeclared let/const (as this operation is initializing the let/const)
        PropertyOperation_AllowUndecl                   = 0x80,

        // No need to check for undeclared let/const in case of console scope (as this operation is initializing the let/const)
        PropertyOperation_AllowUndeclInConsoleScope     = 0x100,

        PropertyOperation_ThrowIfNonWritable            = 0x200,

        // This will be passed during delete operation. This will make the delete operation throw when the property not configurable.
        PropertyOperation_ThrowOnDeleteIfNotConfig      = 0x400,
    };

    class RecyclableObject : public FinalizableObject
    {
        friend class JavascriptOperators;
#if DBG
    public:
        DECLARE_VALIDATE_VTABLE_REGISTERED_NOBASE(RecyclableObject);
#endif
#if DBG || defined(PROFILE_TYPES)
    protected:
        RecyclableObject(DynamicType * type, ScriptContext * scriptContext);

    private:
        void RecordAllocation(ScriptContext * scriptContext);
#endif
    protected:
        Field(Type *) type;
        DEFINE_VTABLE_CTOR_NOBASE(RecyclableObject);

        virtual RecyclableObject* GetPrototypeSpecial();

    public:
        static bool Is(Var aValue);
        static RecyclableObject* FromVar(Var varValue);
        RecyclableObject(Type * type);
#if DBG_EXTRAFIELD
        // This dtor should only be call when OOM occurs and RecyclableObject ctor has completed
        // as the base class, or we have a stack instance
        ~RecyclableObject() { dtorCalled = true; }
#endif
        ScriptContext* GetScriptContext() const;
        TypeId GetTypeId() const;
        RecyclableObject* GetPrototype() const;
        JavascriptMethod GetEntryPoint() const;
        JavascriptLibrary* GetLibrary() const;
        Recycler* GetRecycler() const;
        void SetIsPrototype();

        // Is this object known to have only writable data properties
        // (i.e. no accessors or non-writable properties)?
        bool HasOnlyWritableDataProperties();

        void ClearWritableDataOnlyDetectionBit();
        bool IsWritableDataOnlyDetectionBitSet();

        inline Type * GetType() const { return type; }

        // In order to avoid a branch, every object has an entry point if it gets called like a
        // function - however, if it can't be called like a function, it's set to DefaultEntryPoint
        // which will emit an error.
        static Var DefaultEntryPoint(RecyclableObject* function, CallInfo callInfo, ...);

        virtual PropertyId GetPropertyId(PropertyIndex index) {TRACE_IT(66956); return Constants::NoProperty; }
        virtual PropertyId GetPropertyId(BigPropertyIndex index) {TRACE_IT(66957); return Constants::NoProperty; }
        virtual PropertyIndex GetPropertyIndex(PropertyId propertyId) {TRACE_IT(66958); return Constants::NoSlot; }
        virtual int GetPropertyCount() { return 0; }
        virtual BOOL HasProperty(PropertyId propertyId);
        virtual BOOL HasOwnProperty( PropertyId propertyId);
        virtual BOOL HasOwnPropertyNoHostObject( PropertyId propertyId);
        virtual BOOL HasOwnPropertyCheckNoRedecl( PropertyId propertyId) {TRACE_IT(66960); Assert(FALSE); return FALSE; }
        virtual BOOL UseDynamicObjectForNoHostObjectAccess() {TRACE_IT(66961); return FALSE; }
        virtual DescriptorFlags GetSetter(PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext) {TRACE_IT(66962); return None; }
        virtual DescriptorFlags GetSetter(JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext) {TRACE_IT(66963); return None; }
        virtual BOOL GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext);
        virtual BOOL GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext);
        virtual BOOL GetInternalProperty(Var instance, PropertyId internalPropertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext);
        virtual BOOL GetAccessors(PropertyId propertyId, Var* getter, Var* setter, ScriptContext * requestContext);
        virtual BOOL GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext);
        virtual BOOL SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info);
        virtual BOOL SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info);
        virtual BOOL SetInternalProperty(PropertyId internalPropertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info);
        virtual BOOL InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags = PropertyOperation_None, PropertyValueInfo* info = NULL);
        virtual BOOL EnsureProperty(PropertyId propertyId);
        virtual BOOL EnsureNoRedeclProperty(PropertyId propertyId);
        virtual BOOL SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags = PropertyOperation_None, SideEffects possibleSideEffects = SideEffects_Any);
        virtual BOOL InitPropertyScoped(PropertyId propertyId, Var value);
        virtual BOOL InitFuncScoped(PropertyId propertyId, Var value);
        virtual BOOL DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags);
        virtual BOOL DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags);
        virtual BOOL IsFixedProperty(PropertyId propertyId);
        virtual BOOL HasItem(uint32 index);
        virtual BOOL HasOwnItem(uint32 index);
        virtual BOOL GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext);
        virtual BOOL GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext);
        virtual DescriptorFlags GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext) {TRACE_IT(66964); return None; }
        virtual BOOL SetItem(uint32 index, Var value, PropertyOperationFlags flags);
        virtual BOOL DeleteItem(uint32 index, PropertyOperationFlags flags);
        virtual BOOL GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache = nullptr);
        virtual BOOL ToPrimitive(JavascriptHint hint, Var* value, ScriptContext * requestContext);
        virtual BOOL SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags = PropertyOperation_None);
        virtual BOOL Equals(__in Var other, __out BOOL* value, ScriptContext* requestContext);
        virtual BOOL StrictEquals(__in Var other, __out BOOL* value, ScriptContext* requestContext);
        virtual BOOL IsWritable(PropertyId propertyId) {TRACE_IT(66965); return false; }
        virtual BOOL IsConfigurable(PropertyId propertyId) {TRACE_IT(66966); return false; }
        virtual BOOL IsEnumerable(PropertyId propertyId) {TRACE_IT(66967); return false; }
        virtual BOOL IsExtensible() {TRACE_IT(66968); return false; }
        virtual BOOL IsProtoImmutable() const {TRACE_IT(66969); return false; }
        virtual BOOL PreventExtensions() {TRACE_IT(66970); return false; };     // Sets [[Extensible]] flag of instance to false
        virtual void ThrowIfCannotDefineProperty(PropertyId propId, const PropertyDescriptor& descriptor);
        virtual void ThrowIfCannotGetOwnPropertyDescriptor(PropertyId propId) {TRACE_IT(66971);}
        virtual BOOL GetDefaultPropertyDescriptor(PropertyDescriptor& descriptor);
        virtual BOOL Seal() {TRACE_IT(66972); return false; }                   // Seals the instance, no additional property can be added or deleted
        virtual BOOL Freeze() {TRACE_IT(66973); return false; }                 // Freezes the instance, no additional property can be added or deleted or written
        virtual BOOL IsSealed() {TRACE_IT(66974); return false; }
        virtual BOOL IsFrozen() {TRACE_IT(66975); return false; }
        virtual BOOL SetWritable(PropertyId propertyId, BOOL value) {TRACE_IT(66976); return false; }
        virtual BOOL SetConfigurable(PropertyId propertyId, BOOL value) {TRACE_IT(66977); return false; }
        virtual BOOL SetEnumerable(PropertyId propertyId, BOOL value) {TRACE_IT(66978); return false; }
        virtual BOOL SetAttributes(PropertyId propertyId, PropertyAttributes attributes) {TRACE_IT(66979); return false; }

        virtual BOOL GetSpecialPropertyName(uint32 index, Var *propertyName, ScriptContext * requestContext) {TRACE_IT(66980); return false; }
        virtual uint GetSpecialPropertyCount() const {TRACE_IT(66981); return 0; }
        virtual PropertyId const * GetSpecialPropertyIds() const {TRACE_IT(66982); return nullptr; }
        virtual RecyclableObject* GetThisObjectOrUnWrap(); // Due to the withScope object there are times we need to unwrap

        virtual BOOL HasInstance(Var instance, ScriptContext* scriptContext, IsInstInlineCache* inlineCache = NULL);

        BOOL SkipsPrototype() const;
        BOOL CanHaveInterceptors() const;
        BOOL IsExternal() const;
        // Used only in JsVarToExtension where it may be during dispose and the type is not available
        virtual BOOL IsExternalVirtual() const {TRACE_IT(66983); return FALSE; }

        virtual RecyclableObject* GetConfigurablePrototype(ScriptContext * requestContext) {TRACE_IT(66984); return GetPrototype(); }
        virtual Js::JavascriptString* GetClassName(ScriptContext * requestContext);
        virtual RecyclableObject* GetProxiedObjectForHeapEnum();

#if DBG
        virtual bool CanStorePropertyValueDirectly(PropertyId propertyId, bool allowLetConst) {TRACE_IT(66985); Assert(false); return false; };
#endif

        virtual void RemoveFromPrototype(ScriptContext * requestContext) { AssertMsg(false, "Shouldn't call this implementation."); }
        virtual void AddToPrototype(ScriptContext * requestContext) { AssertMsg(false, "Shouldn't call this implementation."); }
        virtual void SetPrototype(RecyclableObject* newPrototype) { AssertMsg(false, "Shouldn't call this implementation."); }

        virtual BOOL ToString(Js::Var* value, Js::ScriptContext* scriptContext) { AssertMsg(FALSE, "Do not use this function."); return false; }

        // don't need cross-site: in HostDispatch it's IDispatchEx based; in CustomExternalObject we have marshalling code explicitly.
        virtual Var GetNamespaceParent(Js::Var aChild) {TRACE_IT(66986); return nullptr; }
        virtual HRESULT QueryObjectInterface(REFIID riid, void **ppvObj);

        virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext);
        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext);
        virtual RecyclableObject* ToObject(ScriptContext * requestContext);
        virtual Var GetTypeOfString(ScriptContext* requestContext);

        // don't need cross-site: only supported in HostDispatch.
        virtual Var InvokePut(Arguments args);
        virtual BOOL GetRemoteTypeId(TypeId* typeId);

        // Only implemented by the HostDispatch object for cross-thread support
        // Only supports a subset of entry points to be called remotely.
        // For a list of supported entry points see the BuiltInOperation enum defined in JscriptInfo.idl
        virtual BOOL InvokeBuiltInOperationRemotely(JavascriptMethod entryPoint, Arguments args, Var* result) {TRACE_IT(66987); return FALSE; };

        // don't need cross-site: only supported in HostDispatch.
        virtual DynamicObject* GetRemoteObject();

        // don't need cross-site: get the HostDispatch for global object/module root. don't need marshalling.
        virtual Var GetHostDispatchVar();

        virtual RecyclableObject * CloneToScriptContext(ScriptContext* requestContext);

        // If dtor is called, that means that OOM happened (mostly), then the vtable might not be initialized
        // to the base class', so we can't assert.
        virtual void Finalize(bool isShutdown) override {
#ifdef DBG_EXTRAFIELD
            AssertMsg(dtorCalled, "Can't allocate a finalizable object without implementing Finalize");
#endif
        }
        virtual void Dispose(bool isShutdown) override {
#ifdef DBG_EXTRAFIELD
            AssertMsg(dtorCalled, "Can't allocate a finalizable object without implementing Dispose");
#endif
        }
        virtual void Mark(Recycler *recycler) override { AssertMsg(false, "Mark called on object that isn't TrackableObject"); }

        static uint32 GetOffsetOfType() {TRACE_IT(66988); return offsetof(RecyclableObject, type); }

        virtual void InvalidateCachedScope() {TRACE_IT(66989); return; }
        virtual BOOL HasDeferredTypeHandler() const {TRACE_IT(66990); return false; }
#if DBG
    public:
        // Used to Assert that the object may safely be cast to a DynamicObject
        virtual bool DbgIsDynamicObject() const {TRACE_IT(66991); return false; }
        virtual BOOL DbgSkipsPrototype() const {TRACE_IT(66992); return FALSE; }
        virtual BOOL DbgCanHaveInterceptors() const {TRACE_IT(66993); return false; }
#endif
#if defined(PROFILE_RECYCLER_ALLOC) && defined(RECYCLER_DUMP_OBJECT_GRAPH)
    public:
        static bool DumpObjectFunction(type_info const * typeinfo, bool isArray, void * objectAddress);
#endif

#if ENABLE_TTD
    public:
        //Do any additional marking that is needed for a TT snapshotable object
        virtual void MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
        {TRACE_IT(66994);
            ;
        }

        //Do the path processing for our "core path" computation to find wellknown objects in a brute force manner.
        virtual void ProcessCorePaths()
        {TRACE_IT(66995);
            ;
        }

        //Get the SnapObjectType tag that this object maps to
        virtual TTD::NSSnapObjects::SnapObjectType GetSnapTag_TTD() const;

        //Do the extraction of the SnapObject for each of the kinds of objects in the heap
        virtual void ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc);
#endif

    private:

#if DBG_EXTRAFIELD
        bool dtorCalled;
#endif
        friend class LowererMD;
        friend class LowererMDArch;
        friend struct InlineCache;

#ifdef HEAP_ENUMERATION_VALIDATION
    private:
        UINT m_heapEnumValidationCookie;
    public:
        void SetHeapEnumValidationCookie(int cookie ) {TRACE_IT(66996); m_heapEnumValidationCookie = cookie; }
        int GetHeapEnumValidationCookie() {TRACE_IT(66997); return m_heapEnumValidationCookie; }
#endif
    };
}
