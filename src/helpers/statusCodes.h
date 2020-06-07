#pragma once

#ifndef NEXPRESS_STATUSCODES_H
#define NEXPRESS_STATUSCODES_H

namespace nex {

    inline const char* getStatusTextByCode(uint32_t code) {
        switch (code) {
            case 202:
                return "Accepted";
            case 502:
                return "Bad Gateway";
            case 400:
                return "Bad Request";
            case 409:
                return "Conflict";
            case 100:
                return "Continue";
            case 201:
                return "Created";
            case 417:
                return "Expectation Failed";
            case 424:
                return "Failed Dependency";
            case 403:
                return "Forbidden";
            case 504:
                return "Gateway Timeout";
            case 410:
                return "Gone";
            case 505:
                return "HTTP Version Not Supported";
            case 418:
                return "I'm a teapot";
            case 419:
                return "Insufficient Space on Resource";
            case 507:
                return "Insufficient Storage";
            case 500:
                return "Server Error";
            case 411:
                return "Length Required";
            case 423:
                return "Locked";
            case 420:
                return "Method Failure";
            case 405:
                return "Method Not Allowed";
            case 301:
                return "Moved Permanently";
            case 302:
                return "Moved Temporarily";
            case 207:
                return "Multi-Status";
            case 300:
                return "Multiple Choices";
            case 511:
                return "Network Authentication Required";
            case 204:
                return "No Content";
            case 203:
                return "Non Authoritative Information";
            case 406:
                return "Not Acceptable";
            case 404:
                return "Not Found";
            case 501:
                return "Not Implemented";
            case 304:
                return "Not Modified";
            case 200:
                return "OK";
            case 206:
                return "Partial Content";
            case 402:
                return "Payment Required";
            case 308:
                return "Permanent Redirect";
            case 412:
                return "Precondition Failed";
            case 428:
                return "Precondition Required";
            case 102:
                return "Processing";
            case 407:
                return "Proxy Authentication Required";
            case 431:
                return "Request Header Fields Too Large";
            case 408:
                return "Request Timeout";
            case 413:
                return "Request Entity Too Large";
            case 414:
                return "Request-URI Too Long";
            case 416:
                return "Requested Range Not Satisfiable";
            case 205:
                return "Reset Content";
            case 303:
                return "See Other";
            case 503:
                return "Service Unavailable";
            case 101:
                return "Switching Protocols";
            case 307:
                return "Temporary Redirect";
            case 429:
                return "Too Many Requests";
            case 401:
                return "Unauthorized";
            case 422:
                return "Unprocessable Entity";
            case 415:
                return "Unsupported Media Type";
            case 305:
                return "Use Proxy";
            default:
                return "Unknown";
        }
    }

}
#endif //NEXPRESS_STATUSCODES_H
