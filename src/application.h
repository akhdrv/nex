#pragma once

#include <node.h>
#include <node_object_wrap.h>

#include "commonHeaders.h"
#include "embeddedHttp.h"
#include "router.h"

namespace nex {
    using v8::Persistent;
    using v8::Local;
    using v8::Isolate;
    using v8::Value;
    using v8::Context;
    using v8::Object;
    using v8::Function;
    using v8::FunctionTemplate;

    class Application : public RouterMethods {

    public:
        static void Init(v8::Isolate* isolate);
        static void NewInstance(const v8::FunctionCallbackInfo<v8::Value>& args);

        void listen(const std::string& ip, uint16_t port);
        void close();

    private:
        static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

        static void Listen(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Set(const v8::FunctionCallbackInfo<v8::Value>& args);

        static v8::Global<v8::Function> constructor;

        std::shared_ptr<EmbeddedHttp> http{nullptr};
        std::shared_ptr<HttpServerConfig> httpConfig{nullptr};

        v8::Isolate* isolate = nullptr;
    };

}

