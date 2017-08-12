/*
    This file is part of the WebKit open source project.
    This file has been generated by generate-bindings.pl. DO NOT MODIFY!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#pragma once

#include "JSDOMWrapper.h"
#include "TestOverloadedConstructors.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

class JSTestOverloadedConstructors : public JSDOMWrapper<TestOverloadedConstructors> {
public:
    using Base = JSDOMWrapper<TestOverloadedConstructors>;
    static JSTestOverloadedConstructors* create(JSC::Structure* structure, JSDOMGlobalObject* globalObject, Ref<TestOverloadedConstructors>&& impl)
    {
        JSTestOverloadedConstructors* ptr = new (NotNull, JSC::allocateCell<JSTestOverloadedConstructors>(globalObject->vm().heap)) JSTestOverloadedConstructors(structure, *globalObject, WTFMove(impl));
        ptr->finishCreation(globalObject->vm());
        return ptr;
    }

    static JSC::JSObject* createPrototype(JSC::VM&, JSC::JSGlobalObject*);
    static JSC::JSObject* prototype(JSC::VM&, JSC::JSGlobalObject*);
    static TestOverloadedConstructors* toWrapped(JSC::JSValue);
    static void destroy(JSC::JSCell*);

    DECLARE_INFO;

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

    static JSC::JSValue getConstructor(JSC::VM&, const JSC::JSGlobalObject*);
protected:
    JSTestOverloadedConstructors(JSC::Structure*, JSDOMGlobalObject&, Ref<TestOverloadedConstructors>&&);

    void finishCreation(JSC::VM&);
};

class JSTestOverloadedConstructorsOwner : public JSC::WeakHandleOwner {
public:
    virtual bool isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown>, void* context, JSC::SlotVisitor&);
    virtual void finalize(JSC::Handle<JSC::Unknown>, void* context);
};

inline JSC::WeakHandleOwner* wrapperOwner(DOMWrapperWorld&, TestOverloadedConstructors*)
{
    static NeverDestroyed<JSTestOverloadedConstructorsOwner> owner;
    return &owner.get();
}

inline void* wrapperKey(TestOverloadedConstructors* wrappableObject)
{
    return wrappableObject;
}

JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, TestOverloadedConstructors&);
inline JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, TestOverloadedConstructors* impl) { return impl ? toJS(state, globalObject, *impl) : JSC::jsNull(); }
JSC::JSValue toJSNewlyCreated(JSC::ExecState*, JSDOMGlobalObject*, Ref<TestOverloadedConstructors>&&);
inline JSC::JSValue toJSNewlyCreated(JSC::ExecState* state, JSDOMGlobalObject* globalObject, RefPtr<TestOverloadedConstructors>&& impl) { return impl ? toJSNewlyCreated(state, globalObject, impl.releaseNonNull()) : JSC::jsNull(); }

template<> struct JSDOMWrapperConverterTraits<TestOverloadedConstructors> {
    using WrapperClass = JSTestOverloadedConstructors;
    using ToWrappedReturnType = TestOverloadedConstructors*;
};

} // namespace WebCore
