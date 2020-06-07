#include "application.h"

namespace nex {

    v8::Global<v8::Function> Application::constructor;

    void Application::Init(v8::Isolate *isolate) {
        Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
        tpl->SetClassName(v8::String::NewFromUtf8(
                isolate, "Nexpress", v8::NewStringType::kNormal).ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        NODE_SET_PROTOTYPE_METHOD(tpl, "listen", Listen);
        NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);
        NODE_SET_PROTOTYPE_METHOD(tpl, "use", Use);
        NODE_SET_PROTOTYPE_METHOD(tpl, "set", Set);

        Local<v8::Context> context = isolate->GetCurrentContext();
        constructor.Reset(isolate, tpl->GetFunction(context).ToLocalChecked());

        node::AddEnvironmentCleanupHook(isolate, [](void*) {
            constructor.Reset();
        }, nullptr);
    }

    void Application::NewInstance(const v8::FunctionCallbackInfo<v8::Value> &args) {
        Isolate* isolate = args.GetIsolate();

        Local<Function> cons = Local<Function>::New(isolate, constructor);
        Local<Context> context = isolate->GetCurrentContext();
        Local<Object> instance = cons->NewInstance(context).ToLocalChecked();

        args.GetReturnValue().Set(instance);
    }

    void Application::New(const v8::FunctionCallbackInfo<v8::Value> &args) {
        Isolate* isolate = args.GetIsolate();
        Local<Context> context = isolate->GetCurrentContext();

        if (args.IsConstructCall()) {
            auto app = new Application;
            app->routerInstance = std::shared_ptr<Router>(new Router(isolate));
            app->isolate = isolate;

            app->Wrap(args.This());
            args.GetReturnValue().Set(args.This());
        } else {
            Local<Function> cons = Local<Function>::New(isolate, constructor);
            Local<Object> instance = cons->NewInstance(context).ToLocalChecked();
            args.GetReturnValue().Set(instance);
        }
    }

    void Application::Set(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();

        if (args[0].IsEmpty() || args[1].IsEmpty() || !args[0]->IsString()) {
            return;
        }

        v8::String::Utf8Value settingNameInternal(isolate, args[0]);
        std::string settingName(*settingNameInternal);

        auto app = ObjectWrap::Unwrap<Application>(args.Holder());

        if (!app->httpConfig) {
            app->httpConfig = std::make_shared<HttpServerConfig>();
        }
        auto httpConfig = app->httpConfig;

        if (settingName == "responseTimeout") {
            if (!args[1]->IsNumber()) {
                return;
            }
            auto value = static_cast<uint32_t>(args[1].As<v8::Number>()->Value());
            httpConfig->responseTimeout = value;
            return;
        }

        if (settingName == "requestTimeout") {
            if (!args[1]->IsNumber()) {
                return;
            }
            auto value = static_cast<uint32_t>(args[1].As<v8::Number>()->Value());
            httpConfig->requestTimeout = value;
            return;
        }

        if (settingName == "keepAliveTimeout") {
            if (!args[1]->IsNumber()) {
                return;
            }
            auto value = static_cast<uint32_t>(args[1].As<v8::Number>()->Value());
            httpConfig->keepAliveTimeout = value;
            return;
        }

        if (settingName == "maxRequestBodyLength") {
            if (!args[1]->IsNumber()) {
                return;
            }
            auto value = static_cast<uint32_t>(args[1].As<v8::Number>()->Value());
            httpConfig->maxRequestBodyLength = value;
            return;
        }

        if (settingName == "maxRequestsPerConnection") {
            if (!args[1]->IsNumber()) {
                return;
            }
            auto value = static_cast<uint32_t>(args[1].As<v8::Number>()->Value());
            httpConfig->maxRequestsPerConnection = value;
            return;
        }

        if (settingName == "maxPathLength") {
            if (!args[1]->IsNumber()) {
                return;
            }
            auto value = static_cast<uint32_t>(args[1].As<v8::Number>()->Value());
            httpConfig->maxPathLength = value;
            return;
        }

        if (settingName == "keepAlive") {
            if (!args[1]->IsBoolean()) {
                return;
            }
            auto value = args[1].As<v8::Boolean>()->Value();
            httpConfig->persistentConnections = value;
            return;
        }

        if (settingName == "protocol") {
            if (!args[1]->IsString()) {
                return;
            }
            v8::String::Utf8Value internalValue(isolate, args[0]);
            std::string value(*settingNameInternal);
            httpConfig->protocol = value;
            return;
        }
    }

    void Application::Listen(const v8::FunctionCallbackInfo<v8::Value> &args) {
        v8::Isolate* isolate = args.GetIsolate();

        if (args[0].IsEmpty() || args[1].IsEmpty() || !args[0]->IsString() || !args[1]->IsNumber()) {
            return;
        }

        v8::String::Utf8Value ipAddressInternal(isolate, args[0]);
        std::string ipAddress(*ipAddressInternal);
        int port;

        auto portInternal = args[1].As<v8::Number>();

        if (portInternal.IsEmpty()) {
            return;
        }

        port = static_cast<uint16_t>(portInternal->Value());

        auto app = ObjectWrap::Unwrap<Application>(args.Holder());
        app->listen(ipAddress, port);
    }

    void Application::Close(const v8::FunctionCallbackInfo<v8::Value>& args) {
        auto app = ObjectWrap::Unwrap<Application>(args.Holder());
        app->close();
    }

    void Application::listen(const std::string &ip, uint16_t port) {
        Ref();
        if (!httpConfig) {
            httpConfig = std::make_shared<HttpServerConfig>();
        }

        if (!http) {
            http = EmbeddedHttp::createInstance(routerInstance, isolate, nullptr, httpConfig);
        }

        http->listen(ip, port);
    }

    void Application::close() {
        Unref();
        if (!http) {
            return;
        }

        http->close();
    }

}
