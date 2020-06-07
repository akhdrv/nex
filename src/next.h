#pragma once

#ifndef NEXPRESS_NEXT_H
#define NEXPRESS_NEXT_H

#include <node.h>
#include <node_object_wrap.h>

#include "commonHeaders.h"

namespace nex {

class NextWrap;

class NextObject {

public:
    NextObject() = default;
    NextObject(std::function<void()> next,
               std::function<void()> nextRoute,
               std::function<void(std::string)> error,
               v8::Isolate* isolate
    );

    void next();

    void nextRoute();

    void error(const std::string& err);

    v8::Persistent<v8::Object>& getJsObject();

    void operator() ();

    ~NextObject();

    std::function<void()> nextFn;
    std::function<void()> nextRouteFn;
    std::function<void(std::string)> errorFn;
private:
    friend class NextWrap;
    v8::Isolate* isolate = nullptr;

    NextWrap* jsObj = nullptr;
};


class NextWrap final: public node::ObjectWrap {
public:
    void invalidate();

    static void Init(v8::Isolate* isolate);
    static NextWrap* NewInstance(v8::Isolate* isolate, NextObject* instance);

    ~NextWrap() final;
private:
    static v8::Global<v8::Function> constructor;
    static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

    NextWrap() = default;

    static void Next(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void NextRoute(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void NextError(const v8::FunctionCallbackInfo<v8::Value>& args);

    NextObject* instance = nullptr;
    bool isValid = true;
};

}

#endif //NEXPRESS_NEXT_H
