#pragma once

#ifndef NEXPRESS_METHODS_H
#define NEXPRESS_METHODS_H

#include <string>
#include <exception>

namespace nex {
    constexpr unsigned int str2int(const char* str, unsigned int h = 0) {
        return !str[h] ? 5381U : (str2int(str, h+1U) * 33U) ^ static_cast<unsigned char>(str[h]);
    }

    enum HttpMethod {
        ALL = 0,
        ACL = str2int("ACL"),
        BIND = str2int("BIND"),
        CHECKOUT = str2int("CHECKOUT"),
        CONNECT = str2int("CONNECT"),
        COPY = str2int("COPY"),
        DELETE = str2int("DELETE"),
        GET = str2int("GET"),
        HEAD = str2int("HEAD"),
        LINK = str2int("LINK"),
        LOCK = str2int("LOCK"),
        M_SEARCH = str2int("M-SEARCH"),
        MERGE = str2int("MERGE"),
        MKACTIVITY = str2int("MKACTIVITY"),
        MKCALENDAR = str2int("MKCALENDAR"),
        MKCOL = str2int("MKCOL"),
        MOVE = str2int("MOVE"),
        NOTIFY = str2int("NOTIFY"),
        OPTIONS = str2int("OPTIONS"),
        PATCH = str2int("PATCH"),
        POST = str2int("POST"),
        PROPFIND = str2int("PROPFIND"),
        PROPPATCH = str2int("PROPPATCH"),
        PURGE = str2int("PURGE"),
        PUT = str2int("PUT"),
        REBIND = str2int("REBIND"),
        REPORT = str2int("REPORT"),
        SEARCH = str2int("SEARCH"),
        SOURCE = str2int("SOURCE"),
        SUBSCRIBE = str2int("SUBSCRIBE"),
        TRACE = str2int("TRACE"),
        UNBIND = str2int("UNBIND"),
        UNLINK = str2int("UNLINK"),
        UNLOCK = str2int("UNLOCK"),
        UNSUBSCRIBE = str2int("UNSUBSCRIBE")
    };

    static HttpMethod ALL_HTTP_METHODS[] = {
        ACL, BIND, CHECKOUT, CONNECT,
        COPY, DELETE, GET, HEAD,
        LINK, LOCK, M_SEARCH, MERGE,
        MKACTIVITY, MKCALENDAR, MKCOL, MOVE,
        NOTIFY, OPTIONS, PATCH, POST,
        PROPFIND, PROPPATCH, PURGE, PUT,
        REBIND, REPORT, SEARCH, SOURCE,
        SUBSCRIBE, TRACE, UNBIND, UNLINK,
        UNLOCK, UNSUBSCRIBE
    };

    inline HttpMethod parseMethod(const std::string& method) {
        const char * ret = nullptr;
        HttpMethod retMethod;

        switch (str2int(method.c_str())) {

            case HttpMethod::ACL:
                ret = "ACL";
                retMethod = HttpMethod::ACL;
                break;

            case HttpMethod::BIND:
                ret = "BIND";
                retMethod = HttpMethod::BIND;
                break;

            case HttpMethod::CHECKOUT:
                ret = "CHECKOUT";
                retMethod = HttpMethod::CHECKOUT;
                break;

            case HttpMethod::CONNECT:
                ret = "CONNECT";
                retMethod = HttpMethod::CONNECT;
                break;

            case HttpMethod::COPY:
                ret = "COPY";
                retMethod = HttpMethod::COPY;
                break;

            case HttpMethod::DELETE:
                ret = "DELETE";
                retMethod = HttpMethod::DELETE;
                break;

            case HttpMethod::GET:
                ret = "GET";
                retMethod = HttpMethod::GET;
                break;

            case HttpMethod::HEAD:
                ret = "HEAD";
                retMethod = HttpMethod::HEAD;
                break;

            case HttpMethod::LINK:
                ret = "LINK";
                retMethod = HttpMethod::LINK;
                break;

            case HttpMethod::LOCK:
                ret = "LOCK";
                retMethod = HttpMethod::LOCK;
                break;

            case HttpMethod::M_SEARCH:
                ret = "M-SEARCH";
                retMethod = HttpMethod::M_SEARCH;
                break;

            case HttpMethod::MERGE:
                ret = "MERGE";
                retMethod = HttpMethod::MERGE;
                break;

            case HttpMethod::MKACTIVITY:
                ret = "MKACTIVITY";
                retMethod = HttpMethod::MKACTIVITY;
                break;

            case HttpMethod::MKCALENDAR:
                ret = "MKCALENDAR";
                retMethod = HttpMethod::MKCALENDAR;
                break;

            case HttpMethod::MKCOL:
                ret = "MKCOL";
                retMethod = HttpMethod::MKCOL;
                break;

            case HttpMethod::MOVE:
                ret = "MOVE";
                retMethod = HttpMethod::MOVE;
                break;

            case HttpMethod::NOTIFY:
                ret = "NOTIFY";
                retMethod = HttpMethod::NOTIFY;
                break;

            case HttpMethod::OPTIONS:
                ret = "OPTIONS";
                retMethod = HttpMethod::OPTIONS;
                break;

            case HttpMethod::PATCH:
                ret = "PATCH";
                retMethod = HttpMethod::PATCH;
                break;

            case HttpMethod::POST:
                ret = "POST";
                retMethod = HttpMethod::POST;
                break;

            case HttpMethod::PROPFIND:
                ret = "PROPFIND";
                retMethod = HttpMethod::PROPFIND;
                break;

            case HttpMethod::PROPPATCH:
                ret = "PROPPATCH";
                retMethod = HttpMethod::PROPPATCH;
                break;

            case HttpMethod::PURGE:
                ret = "PURGE";
                retMethod = HttpMethod::PURGE;
                break;

            case HttpMethod::PUT:
                ret = "PUT";
                retMethod = HttpMethod::PUT;
                break;

            case HttpMethod::REBIND:
                ret = "REBIND";
                retMethod = HttpMethod::REBIND;
                break;

            case HttpMethod::REPORT:
                ret = "REPORT";
                retMethod = HttpMethod::REPORT;
                break;

            case HttpMethod::SEARCH:
                ret = "SEARCH";
                retMethod = HttpMethod::SEARCH;
                break;

            case HttpMethod::SOURCE:
                ret = "SOURCE";
                retMethod = HttpMethod::SOURCE;
                break;

            case HttpMethod::SUBSCRIBE:
                ret = "SUBSCRIBE";
                retMethod = HttpMethod::SUBSCRIBE;
                break;

            case HttpMethod::TRACE:
                ret = "TRACE";
                retMethod = HttpMethod::TRACE;
                break;

            case HttpMethod::UNBIND:
                ret = "UNBIND";
                retMethod = HttpMethod::UNBIND;
                break;

            case HttpMethod::UNLINK:
                ret = "UNLINK";
                retMethod = HttpMethod::UNLINK;
                break;

            case HttpMethod::UNLOCK:
                ret = "UNLOCK";
                retMethod = HttpMethod::UNLOCK;
                break;

            case HttpMethod::UNSUBSCRIBE:
                ret = "UNSUBSCRIBE";
                retMethod = HttpMethod::UNSUBSCRIBE;
                break;
        }

        if (ret == nullptr || ::strcmp(ret, method.c_str()) != 0)
            throw std::runtime_error("Unknown HTTP Method");

        return retMethod;
    }

    inline std::string methodToString(HttpMethod method) {
        switch (method) {
            case HttpMethod::ACL:
                return "ACL";
            case HttpMethod::BIND:
                return "BIND";
            case HttpMethod::CHECKOUT:
                return "CHECKOUT";
            case HttpMethod::CONNECT:
                return "CONNECT";
            case HttpMethod::COPY:
                return "COPY";
            case HttpMethod::DELETE:
                return "DELETE";
            case HttpMethod::GET:
                return "GET";
            case HttpMethod::HEAD:
                return "HEAD";
            case HttpMethod::LINK:
                return "LINK";
            case HttpMethod::LOCK:
                return "LOCK";
            case HttpMethod::M_SEARCH:
                return "M-SEARCH";
            case HttpMethod::MERGE:
                return "MERGE";
            case HttpMethod::MKACTIVITY:
                return "MKACTIVITY";
            case HttpMethod::MKCALENDAR:
                return "MKCALENDAR";
            case HttpMethod::MKCOL:
                return "MKCOL";
            case HttpMethod::MOVE:
                return "MOVE";
            case HttpMethod::NOTIFY:
                return "NOTIFY";
            case HttpMethod::OPTIONS:
                return "OPTIONS";
            case HttpMethod::PATCH:
                return "PATCH";
            case HttpMethod::POST:
                return "POST";
            case HttpMethod::PROPFIND:
                return "PROPFIND";
            case HttpMethod::PROPPATCH:
                return "PROPPATCH";
            case HttpMethod::PURGE:
                return "PURGE";
            case HttpMethod::PUT:
                return "PUT";
            case HttpMethod::REBIND:
                return "REBIND";
            case HttpMethod::REPORT:
                return "REPORT";
            case HttpMethod::SEARCH:
                return "SEARCH";
            case HttpMethod::SOURCE:
                return "SOURCE";
            case HttpMethod::SUBSCRIBE:
                return "SUBSCRIBE";
            case HttpMethod::TRACE:
                return "TRACE";
            case HttpMethod::UNBIND:
                return "UNBIND";
            case HttpMethod::UNLINK:
                return "UNLINK";
            case HttpMethod::UNLOCK:
                return "UNLOCK";
            case HttpMethod::UNSUBSCRIBE:
                return "UNSUBSCRIBE";
            default:
                return "";
        }
    }
}

#endif //NEXPRESS_METHODS_H
