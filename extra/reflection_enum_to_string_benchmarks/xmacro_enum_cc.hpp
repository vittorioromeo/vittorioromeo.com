#pragma once

#define XMACRO_CC_VALUE_(name) name,
#define XMACRO_CC_CASE_(name) \
    case EnumType_::name: return #name;

#define DEFINE_ENUM_CC(EnumName, LIST_MACRO)                                 \
    enum class EnumName { LIST_MACRO(XMACRO_CC_VALUE_) };                    \
    [[nodiscard]] constexpr const char* to_cstr(EnumName e) noexcept         \
    {                                                                        \
        using EnumType_ = EnumName;                                          \
        switch (e)                                                           \
        {                                                                    \
            LIST_MACRO(XMACRO_CC_CASE_)                                      \
        }                                                                    \
        return "<unknown>";                                                  \
    }
