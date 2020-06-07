#pragma once

#include <node.h>
#include <uvw.hpp>
#include <node_object_wrap.h>

#include "commonHeaders.h"

#include "next.h"
#include "request.h"
#include "response.h"

namespace nex {

class Request;
class Response;
class NextObject;

class AbstractMiddleware {
public:
    virtual bool isErrorHandling() = 0;
    virtual void emit(
        std::shared_ptr<Request> req,
        std::shared_ptr<Response> res,
        NextObject& next
    ) = 0;
};

class ApplicationMiddleware : public AbstractMiddleware {
public:
    bool isErrorHandling() final { return errorHandling; }
protected:
    bool errorHandling = false;
};

class PlainMiddleware final : public AbstractMiddleware {
public:
    PlainMiddleware(const v8::Local<v8::Function>& middleware, bool errorHandling, v8::Isolate* isolate);

    bool isErrorHandling() final { return errorHandling; }

    void emit(
        std::shared_ptr<Request> req,
        std::shared_ptr<Response> res,
        NextObject& next
    ) final;

private:

    v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>> callback;
    v8::Isolate* isolate;
    bool errorHandling = false;
};

typedef void(InternalEmit)(
    std::shared_ptr<Request> req,
    std::shared_ptr<Response> res,
    const std::function<void()>& next,
    const std::function<void()>& nextRoute,
    const std::function<void(std::string)>& error
);

typedef bool(InternalIsErrorHandling)();

class NativeLoadedMiddleware final: public AbstractMiddleware {
public:

    explicit NativeLoadedMiddleware(
        const std::shared_ptr<uvw::Loop>& loop,
        const std::string& pathToMiddleware
    );

    bool isErrorHandling() final;

    void emit(
        std::shared_ptr<Request> req,
        std::shared_ptr<Response> res,
        NextObject& next
    ) final;

private:
    InternalEmit* internalEmit = nullptr;
    InternalIsErrorHandling* internalIsErrorHandling = nullptr;

    std::shared_ptr<uvw::SharedLib> lib;
};

class NativeLoadedMiddlewareWrapper : public node::ObjectWrap {
public:
    static void Init(v8::Isolate* isolate);
    static void NewInstance(const v8::FunctionCallbackInfo<v8::Value>& args);
    std::shared_ptr<NativeLoadedMiddleware> getInstance() {
        return instance;
    };
private:
    static v8::Global<v8::Function> constructor;
    static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

    std::shared_ptr<NativeLoadedMiddleware> instance;
};

}

