#pragma once

#include <node.h>
#include <node_object_wrap.h>
#include "commonHeaders.h"
#include "httpConnection.h"

namespace nex {

class HttpConnection;
class RequestWrap;

typedef std::function<void(const std::string& )> DataReceivedCallback;
typedef std::function<void()> DataEndCallback;

class AbstractRequest {
public:
    [[nodiscard]] virtual HttpMethod getHttpMethod() const noexcept = 0;
    [[nodiscard]] virtual const HeaderValue& getHeader(const std::string& name) = 0;
    [[nodiscard]] virtual std::string getHost() = 0;
    [[nodiscard]] virtual std::string getUrl() = 0;
    [[nodiscard]] virtual const std::string& getPath() const noexcept = 0;
    [[nodiscard]] virtual const std::string& getError() const noexcept = 0;
    [[nodiscard]] virtual const std::string& getRelativePath() const noexcept = 0;
    [[nodiscard]] virtual const std::string& getQueryString() const noexcept = 0;
    [[nodiscard]] virtual const QueryParameterValue& getQueryParam(const std::string& name) = 0;
    [[nodiscard]] virtual const RouteParameterValue& getRouteParam(const std::string& name) = 0;
    [[nodiscard]] virtual const CookieValue& getCookie(const std::string& name) = 0;
    [[nodiscard]] virtual v8::Persistent<v8::Object>& getJsObject() = 0;

    [[nodiscard]] virtual const CustomDataValue& getCustomData(const std::string& key) = 0;
    virtual void setCustomData(const std::string& key, const std::string& value) = 0;

    virtual void onData(DataReceivedCallback cb) = 0;
    virtual void onDataEnd(DataEndCallback cb) = 0;
};

class Request final : public AbstractRequest {
public:

    [[nodiscard]] HttpMethod getHttpMethod() const noexcept override;
    [[nodiscard]] const HeaderValue& getHeader(const std::string& name) override;
    [[nodiscard]] std::string getHost() override;
    [[nodiscard]] std::string getUrl() override;
    [[nodiscard]] const std::string& getPath() const noexcept override;
    [[nodiscard]] const std::string& getError() const noexcept override;
    [[nodiscard]] const std::string& getRelativePath() const noexcept override;
    [[nodiscard]] const std::string& getQueryString() const noexcept override;
    [[nodiscard]] const QueryParameterValue& getQueryParam(const std::string& name) override;
    [[nodiscard]] const RouteParameterValue& getRouteParam(const std::string& name) override;
    [[nodiscard]] const CookieValue& getCookie(const std::string& name) override;
    [[nodiscard]] v8::Persistent<v8::Object>& getJsObject() override;

    [[nodiscard]] const CustomDataValue& getCustomData(const std::string& key) override;
    void setCustomData(const std::string& key, const std::string& value) override;

    void onData(DataReceivedCallback cb) override;
    void onDataEnd(DataEndCallback cb) override;

    ~Request();

private:
    friend class HttpConnection;
    friend class Pipeline;
    friend class RequestWrap;

    Request(
        std::shared_ptr<HttpConnection> httpConnection,
        v8::Isolate* isolate,
        HttpMethod method,
        std::string path,
        uint32_t minorVersion
    );

    explicit Request(uint32_t errorStatusCode);

    void appendHeader(const std::string& name, std::string value);
    void setRelativePath(const std::string& relativePath);
    void setRouteParameter(const std::string& name, RouteParameterValue value);
    void appendQueryParam(const std::string& name, std::string value);
    void clearRouteParameters();

    void invalidate();

    void handleData(const std::string& data);
    void handleDataEnd();

    void createJsObject();
    void parseQueryString();
    void parseCookies();
    void prepare();

    std::shared_ptr<HttpConnection> connection{nullptr};

    DataReceivedCallback onDataCallback = nullptr;
    DataEndCallback onDataEndCallback = nullptr;

    bool isAlive = true;

    HttpMethod method = GET;

    HeaderMapping headers;
    QueryParamMapping queryParams;
    RouteParamMapping routeParams;
    CookieMapping cookies;
    CustomDataMapping customData;

    std::string path;
    std::string relativePath;
    std::string basePath;
    std::string host;
    std::string pathWithoutQueryString;
    std::string queryString;
    std::string anchor;
    std::string error;

    bool isQueryStringParsed = false;
    bool areCookiesParsed = false;

    std::string bodyBuffer;
    bool isFullData = false;
    uint32_t bodyOctetsReceived = 0;
    uint32_t contentLength = 0;
    uint32_t requestError = 0;
    int minorVersion = 1;

    v8::Isolate* isolate = nullptr;
    RequestWrap* jsObj = nullptr;
};

class RequestWrap final: public node::ObjectWrap {
public:
    void invalidate();

    static void Init(v8::Isolate* isolate);
    static RequestWrap* NewInstance(v8::Isolate* isolate, Request* instance);

    void setCustomDataToJsObj(const std::string& key, const std::string& data);

    ~RequestWrap() final;
private:

    explicit RequestWrap() = default;

    static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
    static v8::Global<v8::Function> constructor;

    /**
     * Gets Header value
     */
    static void Get(const v8::FunctionCallbackInfo<v8::Value>& args);

    /**
     * For data passing
     */
    static void On(const v8::FunctionCallbackInfo<v8::Value>& args);

    /**
     * For props
     */
    void setFields();

    /**
     * For data transition between native/js middlewares
     */
    static void SetCustomData(const v8::FunctionCallbackInfo<v8::Value>& args);

    bool isValid = true;
    v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>> onDataCallback, onDataEndCallback;
    Request* instance = nullptr;
};

}

