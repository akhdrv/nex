#pragma once

#ifndef NEXPRESS_MISCELLANEOUS_H
#define NEXPRESS_MISCELLANEOUS_H

#include <ctime>
#include <string>
#include <sstream>
#include <algorithm>

namespace nex {

    template <class T>
    inline void noop(T*) noexcept {}

    inline std::string getStandardizedTime(time_t value) {
        char buf[50];
        tm ts = *gmtime(&value);
        strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S GMT", &ts);

        return std::string(buf);
    }

    inline std::string getStandardizedTime() {
        return getStandardizedTime(time(nullptr));
    }

    inline void readBytesFromStream(std::istream &stream, std::string::size_type count, std::string& out) {
        if (!count) {
            return;
        }
        out.clear();
        out.reserve(count);
        std::copy_n(std::istreambuf_iterator(stream), count, std::back_inserter(out));
    }

    struct caseInsensitiveCompare : std::binary_function<std::string, std::string, bool> {
        struct compareCaseInsensitive : public std::binary_function<unsigned char, unsigned char, bool> {
            bool operator()(const unsigned char& first, const unsigned char& second) const {
                return tolower(first) < tolower(second);
            }
        };
        bool operator()(const std::string& first, const std::string& second) const {
            return std::lexicographical_compare
                (first.begin(), first.end(),
                 second.begin(), second.end(),
                 compareCaseInsensitive());
        }
    };

    struct ResponseCookieValue {
        enum SameSiteAttribute: uint8_t {
            None = 0,
            Lax = 1,
            Strict = 2
        };

        std::string domain,
                path;
        std::variant<std::monostate, std::string> value;
        std::variant<std::monostate, time_t> expires;
        std::variant<std::monostate, uint32_t> maxAge;
        bool secure = false,
                httpOnly = false;
        std::variant<std::monostate, SameSiteAttribute> sameSite;

        static std::string serialize(const std::string& name, const ResponseCookieValue& value) {
            std::stringstream output;
            auto val = std::get_if<std::string>(&value.value);

            if (!val) {
                output << name << "=; Expires="
                       << getStandardizedTime(0)
                       << "; Max-Age=0";

                return output.str();
            }

            output << name << '=' << *val;

            if (auto expires = std::get_if<time_t>(&value.expires)) {
                output << "; Expires=" << getStandardizedTime(*expires);
            }

            if (auto maxAge = std::get_if<uint32_t>(&value.maxAge)) {
                output << "; Max-Age=" << *maxAge;
            }

            if (value.domain.length()) {
                output << "; Domain=" << value.domain;
            }

            if (value.path.length()) {
                output << "; Path=" << value.path;
            }

            if (auto sameSite = std::get_if<SameSiteAttribute>(&value.sameSite)) {
                output << "; SameSite=";

                switch (*sameSite) {
                    case SameSiteAttribute::None:
                        output << "None";
                        break;
                    case SameSiteAttribute::Lax:
                        output << "Lax";
                        break;
                    case SameSiteAttribute::Strict:
                        output << "Strict";
                        break;
                }
            }

            if (value.secure) {
                output << "; Secure";
            }

            if (value.httpOnly) {
                output << "; HttpOnly";
            }

            return output.str();
        }
    };

    inline void stringToLower(std::string& str) {
        std::transform(str.begin(), str.end(), str.begin(),
                       [](unsigned char c){ return std::tolower(c); });
    }

}
#endif //NEXPRESS_MISCELLANEOUS_H
