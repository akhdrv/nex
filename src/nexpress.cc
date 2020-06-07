#include <node.h>

#include "router.h"
#include "application.h"


namespace nex {

    void InitAll(v8::Local<v8::Object>& exports, v8::Local<v8::Object> module) {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();

        Application::Init(isolate);
        RouterWrap::Init(isolate);
        ResponseWrap::Init(isolate);
        RequestWrap::Init(isolate);
        NativeLoadedMiddlewareWrapper::Init(isolate);
        NextWrap::Init(isolate);

        v8::HandleScope handle_scope(isolate);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        v8::Local<v8::String> exportsLiteral = v8::String::NewFromUtf8(isolate, "exports",
                v8::NewStringType::kInternalized).ToLocalChecked();
        v8::Local<v8::String> functionNameLiteral = v8::String::NewFromUtf8(isolate, "createApplication",
                v8::NewStringType::kInternalized).ToLocalChecked();

        v8::Local<v8::FunctionTemplate> functionTemplate = v8::FunctionTemplate::New(
                isolate, Application::NewInstance
        );
        v8::Local<v8::Function> exportsFunction = functionTemplate->GetFunction(context).ToLocalChecked();


        NODE_SET_METHOD((v8::Local<v8::Object>)exportsFunction, "Router", RouterWrap::NewInstance);
        NODE_SET_METHOD((v8::Local<v8::Object>)exportsFunction,
                "NativeMiddlewareWrap", NativeLoadedMiddlewareWrapper::NewInstance);

        exportsFunction->SetName(functionNameLiteral);

        module->Set(context, exportsLiteral, exportsFunction).Check();
    }

    NODE_MODULE(NODE_GYP_MODULE_NAME, InitAll)

}