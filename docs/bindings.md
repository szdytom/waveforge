# Implementing a JS Binding Class

## Header (`include/wforge/runtime.h`)

```cpp
// Bindings always use struct (all public) and extends BindingBase CRTP template
struct MyClass final : BindingBase<MyClass> {
    // -- Required --
    static constexpr const char *CLASS_NAME = "MyClass";
    static const CFunctionList PROTO_FIELDS;

	// -- Required for class that can be constructed from JS --
    static constexpr int CTOR_LENGTH = 2;
    static JSValue ctor(
        JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
    ) noexcept;

	// -- Required for class that cannot be constructed from JS --
	static JSValue parentProto(JSContext *ctx) noexcept; // return JS_UNDEFINED for Object as default

    // -- Optional --
    // static const CFunctionList CTOR_FIELDS;
    // static JSValue parentCtor(JSContext *ctx) noexcept;

	// -- Required for class with references to other JS objects --
	// static void gcMark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) noexcept;

    // C++ data members & helpers
    MyClass(/* ... */) noexcept;

	// ... other methods and fields ...
	int compute() const noexcept;
	int foo;
	std::string bar;
};
```

`CTOR_LENGTH` is the minimum number of arguments the JS constructor (i.e. `MyClass::ctor`) expects. You can assume `argc >= CTOR_LENGTH` in the implementation of `ctor`, without checking.

## Implementation (`src/runtime/my_class.cpp`)

```cpp
#include "hacks.h"
#include "helper.h"
#include "wforge/runtime.h"

namespace wf::js {

namespace {

// --- Getters / Setters / Methods ---
// Macros declared in `helper.h`
// Not all overloads are created by now, feel free to add more as needed
WF_JS_DEF_GETTER_I32(MyClass, getFoo, self->foo())
WF_JS_DEF_GETTER_STR(MyClass, getName, self->name.c_str())
WF_JS_DEF_SETTER_I32(MyClass, setFoo, foo)
WF_JS_DEF_SETTER_U8(MyClass, setBar, bar)

WF_JS_METHOD(MyClass, doStuff, {
    // body: self is available, return a JSValue
    return JS_NewInt32(ctx, self->compute());
})

} // namespace

// --- Static PROTO_FIELDS definition ---
static const JSCFunctionListEntry PROTO_FIELDS_DATA[] = {
    cGetSetDef("foo",  MyClass_getFoo,  nullptr),
    cGetSetDef("name", MyClass_getName, nullptr),
    cGetSetDef("bar",  nullptr,         MyClass_setBar),
    cFuncDef("doStuff", 0, MyClass_doStuff),
    cFuncDef("toString", 0, MyClass_toString),
    cFuncDef("[Symbol.toPrimitive]", 1, MyClass_toPrimitive),
};

const CFunctionList MyClass::PROTO_FIELDS{
    PROTO_FIELDS_DATA
};

// --- Constructor ---
JSValue MyClass::ctor(
    JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
    // Validate arguments, return JS_ThrowTypeError on failure
    if (argc < CTOR_LENGTH) {
		return JS_ThrowTypeError(ctx, "MyClass expects at least %d args", CTOR_LENGTH);
    }

    // Create C++ object and attach to JS object
	// Use std::make_unique + release pattern to ensure exception safety
    auto self = std::make_unique<MyClass>(/* ... */);

	// Feel free to set fields of self if constructor didn't cover all initialization
	// unique_ptr makes early return here safe

	// Attach C++ object to JS object. The following 5 line is fixed boilerplate
    JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if 	(JS_IsException(obj)) {
		return obj;
	}
	JS_SetOpaque(obj, self.release());
    return JS_UNDEFINED;
}

} // namespace wf::js
```

## Helper Macros

To simplify the implementation of getters/setters/methods, you can use the helper macros defined in `helper.h`. See the file for detailed usage.

Note on `WF_JS_METHOD`: macros only protect commas inside parentheses, so if you write commas outside parentheses, you need to wrap the somehow. A common error is the capture list of a lambda:

```cpp
WF_JS_METHOD(MyClass, doStuff, {
	auto lambda = [ctx, self]() { /*...*/ }; // The comma in the capture list will break the macro
	return JS_NewInt32(ctx, self->compute()); // Comma in arguments list is fine
})
```

Wrap the lambda in an extra pair of parentheses to fix the issue:

```cpp
WF_JS_METHOD(MyClass, doStuff, {
	auto lambda = ([ctx, self]() { /*...*/ });
	// Now it's fine
})
```

Note: do NOT wrap the whole body in an extra pair of parentheses, because this syntax is only a GNU extension and won't compile on MSVC. Only wrap the lambda or other expressions that contain commas.

## Available Entry Builders (`hacks.h`)

| Function | For |
|---|---|
| `cFuncDef(name, length, func)` | regular method |
| `cGetSetDef(name, getter, setter)` | getter/setter property |
| `propStringDef(name, str, flags)` | static string property |
| `propInt32Def(name, val, flags)` | static int32 property |

Some are not shown in the table, consult `hacks.h` for the full list.

Use `"[Symbol.toPrimitive]"`, `"[Symbol.iterator]"`, etc. as `name` for well-known symbol properties.

## Exception Handling

Throwing an C++ exception from any of the binding methods (constructor, getters/setters, regular methods) is considered a fatal error and will crash the engine. Always throw a JS exception using `JS_ThrowTypeError`. For C++ helper functions that are not directly called by JS, use `std::expected` to propagate errors, instead of throwing C++ exceptions and catching them later.

## Not constructible from JS

If your class is not meant to be constructed from JS (e.g. some iterator), you can omit the `ctor` method and `CTOR_LENGTH` field, and provide a `parentProto` method that returns the prototype object of the parent class (or `JS_UNDEFINED` if the parent is `Object`). In this case, no JS constructor will be created for this class, and you can only create instances of it from C++ and expose them to JS.

## GC Mark and Finalize

If your class keeps references to other JS objects (e.g. storing them in fields), you need to implement `gcMark` and `finalize` methods. For example, if your class has a single `JSValue` field named `foo`, the implementation would look like this:

```cpp
void MyClass::gcMark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) noexcept {
	auto *self = unwrap(rt, val);
	JS_MarkValue(rt, self->foo, mark_func);
}

void MyClass::finalize(JSRuntime *rt, JSValue val) noexcept {
	auto *self = unwrap(rt, val);
	JS_FreeValueRT(rt, self->foo);

	// Default behavior provides by BindingBase
	// But since we override finalize, we need to manually delete self here
	delete self;
}
```

Sometimes you may want to store both a C++ object and its JS wrapper in the same class, this is OK. But be careful, the C++ object must be kept as a "borrowed" reference (e.g. raw pointer, `pro::proxy_view`), not owned reference (e.g. `std::unique_ptr`, `pro::proxy`). The lifecycle of the C++ object is automatically managed by the JS Engine, and you should do nothing to it.

## Wiring bindings into JS contexts

Each binding class exposes a `bindContext(ctx, ns)` method (via `BindingBase`) that registers the class's prototype and constructor on a JS namespace object.

The `BindingList<Ts...>` convenience template (defined in `runtime.h`) folds both `registerClass` and `bindContext` over a type list:

```cpp
// scripted_scene.cpp
using SceneBindings = js::BindingList<
    js::Texture, js::Color //, ... other bindings classes 
>;
```

Usage in a scene context (`scripted_scene.cpp`):

```cpp
// One-time class registration on the engine
// Multiple calls to registerClass with the same class are safe
SceneBindings::registerClass(engine);

// Per-context binding: each JS context gets its own namespace object
// ns = JS_UNDEFINED for binding to global scope
JSValue ns = JS_NewObject(ctx);
SceneBindings::bindContext(ctx, ns);
```

This two-phase approach separates class ID allocation (done once on the `JSRuntime`) from the per-context wiring that attaches constructors and prototypes to a namespace object.

For non class bindings (e.g. plain functions, constants), you can create a JSCFunctionListEntry list and call `JS_SetPropertyFunctionList` directly in your context binding code. You can also mix class bindings and non-class bindings in the same namespace.

Note: the `JS_SetPropertyFunctionList` is lazy evaluated, which means the list must out-live the context. Consider using a `static const` variable to hold constant function lists. Or use the following pattern to create a context-local function list:

```cpp
const JSCFunctionListEntry FUNC_LIST[] = {
	cFuncDef("foo", 0, foo),
	cFuncDef("bar", 0, bar),
};

auto make_unique_array = []<typename T, std::size_t N>(const T(&arr)[N])
	-> std::unique_ptr<T[]> {
	auto ptr = std::make_unique_for_overwrite<T[]>(N);
	std::copy_n(arr, N, ptr.get());
	return ptr;
};

impl.binded_props = make_unique_array(FUNC_LIST);
JS_SetPropertyFunctionList(ctx, ns, impl.binded_props.get(), std::size(FUNC_LIST));
```

In the code snippet, `impl` is a per-context struct that holds context-local data. The struct should live at least as long as the context. The list won't be accessed in the destruction phase, so we don't need to worry about the order of destruction.

This pattern might seem not very performant (allocate on stack, then copy to heap), but it is tested that modern C++ compilers can optimize it as if we directly created the array on the heap. So there should be no runtime overhead compared to a manually written `std::unique_ptr<T[]>` initialization. And it is much more convenient since size is not explicitly specified.
