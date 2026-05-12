#pragma once

#include <string_view>

#define XMACRO_VALUE_(name) name,
#define XMACRO_CASE_(name) \
    case EnumType_::name: return #name;

#define DEFINE_ENUM(EnumName, LIST_MACRO)                                    \
    enum class EnumName { LIST_MACRO(XMACRO_VALUE_) };                       \
    [[nodiscard]] constexpr std::string_view to_string(EnumName e) noexcept  \
    {                                                                        \
        using EnumType_ = EnumName;                                          \
        switch (e)                                                           \
        {                                                                    \
            LIST_MACRO(XMACRO_CASE_)                                         \
        }                                                                    \
        return "<unknown>";                                                  \
    }
