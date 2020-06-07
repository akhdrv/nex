#include "next.h"

namespace nex {

    // NextObject

    NextObject::NextObject(std::function<void()> next, std::function<void()> nextRoute,
                           std::function<void(std::string)> error, v8::Isolate* isolate
       ): nextFn(next), nextRouteFn(nextRoute), errorFn(error), isolate(isolate) {}

    void NextObject::next() {
        try {
            nextFn();
        } catch (const std::exception& e) {
            auto err = std::string(e.what());

            error(err);
        }
    }

    void NextObject::nextRoute() {
        try {
            nextRouteFn();
        } catch (const std::exception& e) {
            auto err = std::string(e.what());

            error(err);
        }
    }

    void NextObject::error(const std::string& err) {
        try {
            errorFn(err);
        } catch (const std::exception& e) {
            auto errInError = std::string(e.what());

            error(errInError);
        }
    }

    v8::Persistent<v8::Object>& NextObject::getJsObject() {
        if (!jsObj) {
            jsObj = NextWrap::NewInstance(isolate, this);
        }

        return jsObj->persistent();
    }

    NextObject::~NextObject() {
        if (jsObj) {
            jsObj->invalidate();
        }
    }

    void NextObject::operator()() {
        next();
    }

    // NextWrap

    v8::Global<v8::Function> NextWrap::constructor;

    void NextWrap::invalidate() {
        if (!isValid) {
            return;
        }

        isValid = false;
        Unref();
    }

    void NextWrap::Init(v8::Isolate* isolate) {
        v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(isolate, New);

        tpl->SetClassName(v8::String::NewFromUtf8(
                isolate, "NNextWrap", v8::NewStringType::kNormal).ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        NODE_SET_PROTOTYPE_METHOD(tpl, "next", Next);
        NODE_SET_PROTOTYPE_METHOD(tpl, "nextRoute", NextRoute);
        NODE_SET_PROTOTYPE_METHOD(tpl, "error", NextError);

        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        constructor.Reset(isolate, tpl->GetFunction(context).ToLocalChecked());

        node::AddEnvironmentCleanupHook(isolate, [](void*) {
            constructor.Reset();
        }, nullptr);
    }

    void NextWrap::New(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        if (args.IsConstructCall()) {
            auto app = new NextWrap;

            app->Wrap(args.This());
            args.GetReturnValue().Set(args.This());
        } else {
            v8::Local<v8::Function> cons = v8::Local<v8::Function>::New(isolate, constructor);
            v8::Local<v8::Object> instance =
                    cons->NewInstance(context).ToLocalChecked();
            args.GetReturnValue().Set(instance);
        }
    }

    NextWrap* NextWrap::NewInstance(v8::Isolate* isolate, NextObject* instance) {
        v8::Local<v8::Function> cons = v8::Local<v8::Function>::New(isolate, constructor);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Object> inst = cons->NewInstance(context).ToLocalChecked();

        auto wrapInstance = ObjectWrap::Unwrap<NextWrap>(inst);
        wrapInstance->instance = instance;
        wrapInstance->Ref();

        return wrapInstance;
    }

    void NextWrap::Next(const v8::FunctionCallbackInfo<v8::Value>& args) {
        auto wrapInstance = ObjectWrap::Unwrap<NextWrap>(args.Holder());

        if (!wrapInstance->isValid) {
            return;
        }

        wrapInstance->instance->next();
    }

    void NextWrap::NextRoute(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        auto wrapInstance = ObjectWrap::Unwrap<NextWrap>(args.Holder());

        if (!wrapInstance->isValid) {
            return;
        }

        wrapInstance->instance->nextRoute();
    }

    void NextWrap::NextError(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        auto wrapInstance = ObjectWrap::Unwrap<NextWrap>(args.Holder());

        if (!wrapInstance->isValid || !args[0]->IsString()) {
            return;
        }

        v8::String::Utf8Value internalError(isolate, args[0]);
        std::string error(*internalError);

        wrapInstance->instance->error(error);
    }

    NextWrap::~NextWrap() {
        if (isValid) {
            instance->jsObj = nullptr;
        }
    }
}
