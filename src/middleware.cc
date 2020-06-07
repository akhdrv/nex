#include "middleware.h"

namespace nex {

// PlainMiddleware

PlainMiddleware::PlainMiddleware(
    const v8::Local<v8::Function>& middleware,
    bool errorHandling,
    v8::Isolate* isolate
): callback(v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>>(isolate, middleware)), isolate(isolate), errorHandling(errorHandling) {}

void PlainMiddleware::emit(
    std::shared_ptr<Request> req,
    std::shared_ptr<Response> res,
    NextObject& next
) {
    if (callback.IsEmpty()) {
        next.error("no callback in plain middleware");
        return;
    }
    v8::HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();

    if (isErrorHandling()) {
        auto error = v8::String::NewFromUtf8(
                isolate, req->getError().c_str(), v8::NewStringType::kNormal).ToLocalChecked();

        v8::Local<v8::Value> argv[] = {
                req->getJsObject().Get(isolate),
                res->getJsObject().Get(isolate),
                next.getJsObject().Get(isolate),
                error
        };

        callback.Get(isolate)->Call(context, v8::Null(isolate), 4, argv);
        return;
    }

    v8::Local<v8::Value> argv[] = {
        req->getJsObject().Get(isolate),
        res->getJsObject().Get(isolate),
        next.getJsObject().Get(isolate)
    };

    callback.Get(isolate)->Call(context, v8::Null(isolate), 3, argv);
}

// NativeLoadedMiddleware

NativeLoadedMiddleware::NativeLoadedMiddleware(
    const std::shared_ptr<uvw::Loop>& loop,
    const std::string& pathToMiddleware
) : lib(uvw::SharedLib::create(loop, pathToMiddleware)) {

    if (!lib || !*lib) {
        std::string err("Library loading failed, error: ");
        err += lib->error();

        throw std::runtime_error(err);
    }

    internalEmit = lib->sym<InternalEmit>("emit");
    internalIsErrorHandling = lib->sym<InternalIsErrorHandling>("isErrorHandling");
}

    bool NativeLoadedMiddleware::isErrorHandling() {
        if (!internalEmit) {
            return false;
        }

        return internalIsErrorHandling();
    }

    void NativeLoadedMiddleware::emit(
        std::shared_ptr<Request> req,
        std::shared_ptr<Response> res,
        NextObject& next
    ) {
        if (!internalEmit) {
            next.error("Couldn't find native middleware emitter");
            return;
        }

        try {
            internalEmit(req, res, next.nextFn, next.nextRouteFn, next.errorFn);
        } catch (const std::exception& e) {
            next.error(e.what());
        }
    }

    // NativeLoadedMiddlewareWrapper

    v8::Global<v8::Function> NativeLoadedMiddlewareWrapper::constructor;

    void NativeLoadedMiddlewareWrapper::Init(v8::Isolate* isolate) {
        v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(isolate, New);
        tpl->SetClassName(v8::String::NewFromUtf8(
                isolate, "NativeMiddleware", v8::NewStringType::kNormal).ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        constructor.Reset(isolate, tpl->GetFunction(context).ToLocalChecked());

        node::AddEnvironmentCleanupHook(isolate, [](void*) {
            constructor.Reset();
        }, nullptr);
    }

    void NativeLoadedMiddlewareWrapper::NewInstance(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();

        const unsigned argc = 1;
        v8::Local<v8::Value> argv[argc] = { args[0] };
        v8::Local<v8::Function> cons = v8::Local<v8::Function>::New(isolate, constructor);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Object> instance =
                cons->NewInstance(context, argc, argv).ToLocalChecked();

        // Flag
        auto nativeMiddlewareFlagKey = v8::String::NewFromUtf8(
                isolate, "__isNexpressNativeMiddleware", v8::NewStringType::kNormal).ToLocalChecked();
        auto nativeMiddlewareFlagValue = v8::Boolean::New(isolate, true);

        instance->Set(nativeMiddlewareFlagKey, nativeMiddlewareFlagValue);

        args.GetReturnValue().Set(instance);
    }

    void NativeLoadedMiddlewareWrapper::New(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        if (args.IsConstructCall()) {
            if (args[0].IsEmpty() || !args[0]->IsString()) {
                isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(
                        isolate, "No native middleware path", v8::NewStringType::kNormal).ToLocalChecked()));
                return;
            }

            auto wrapper = new NativeLoadedMiddlewareWrapper;
            v8::String::Utf8Value pathInternal(isolate, args[0]);
            std::string path = *pathInternal;

            wrapper->instance = std::shared_ptr<NativeLoadedMiddleware>(
                new NativeLoadedMiddleware(uvw::Loop::getDefault(), path)
            );

            wrapper->Wrap(args.This());
            args.GetReturnValue().Set(args.This());
        } else {
            const int argc = 1;
            v8::Local<v8::Value> argv[argc] = { args[0] };
            v8::Local<v8::Function> cons = v8::Local<v8::Function>::New(isolate, constructor);
            v8::Local<v8::Object> instance =
                    cons->NewInstance(context, argc, argv).ToLocalChecked();
            args.GetReturnValue().Set(instance);
        }
    }
}