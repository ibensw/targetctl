#pragma once
#include "systemctl.h"
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

struct ParseError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

template <typename E, typename I, char S> struct BasicDBusMessageReader {
    static std::string stringType()
    {
        return {S};
    }
    using ReturnType = E;
    static std::optional<E> read(DBusMessage &msg)
    {
        I ptr;
        int success = sd_bus_message_read_basic(msg.msg(), S, &ptr);
        if (success > 0) {
            return ptr;
        } else if (success == 0) {
            return std::nullopt;
        } else {
            throw ParseError(strerror(-success));
        }
    }
};

struct ObjectPath : public std::string {
};

template <typename T> struct DBusMessageReader {
    static std::string stringType()
    {
        return "a" + DBusMessageReader<typename T::value_type>::stringType();
    }
    static std::optional<T> read(DBusMessage &msg)
    {
        if (sd_bus_message_enter_container(msg.msg(), 'a',
                                           DBusMessageReader<typename T::value_type>::stringType().c_str()) == 0) {
            throw std::runtime_error("Failed to enter array container");
        }
        T container;
        while (true) {
            auto value = DBusMessageReader<typename T::value_type>::read(msg);
            if (!value.has_value()) {
                break;
            }
            container.push_back(*value);
        }
        if (sd_bus_message_exit_container(msg.msg()) < 0) {
            throw std::runtime_error("Failed to exit container");
        }
        return container;
    }
};

template <> struct DBusMessageReader<std::string> : public BasicDBusMessageReader<std::string, const char *, 's'> {
};
template <> struct DBusMessageReader<ObjectPath> : public BasicDBusMessageReader<ObjectPath, const char *, 'o'> {
};
template <> struct DBusMessageReader<std::byte> : public BasicDBusMessageReader<std::byte, uint8_t, 'y'> {
};
template <> struct DBusMessageReader<bool> : public BasicDBusMessageReader<bool, int, 'b'> {
};
template <> struct DBusMessageReader<int16_t> : public BasicDBusMessageReader<int16_t, int16_t, 'n'> {
};
template <> struct DBusMessageReader<uint16_t> : public BasicDBusMessageReader<uint16_t, uint16_t, 'q'> {
};
template <> struct DBusMessageReader<uint32_t> : public BasicDBusMessageReader<uint32_t, uint32_t, 'u'> {
};
template <> struct DBusMessageReader<int64_t> : public BasicDBusMessageReader<int64_t, int64_t, 'x'> {
};
template <> struct DBusMessageReader<uint64_t> : public BasicDBusMessageReader<uint64_t, uint64_t, 't'> {
};
template <> struct DBusMessageReader<double> : public BasicDBusMessageReader<double, double, 'd'> {
};
