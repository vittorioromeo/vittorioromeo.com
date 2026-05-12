#pragma once

#include <meta>
#include <string_view>
#include <type_traits>

template <typename T>
    requires std::is_enum_v<T>
constexpr std::string_view to_enum_string(T val)
{
    template for (constexpr auto e :
                  std::define_static_array(std::meta::enumerators_of(^^T)))
    {
        if (val == [:e:])
            return std::meta::identifier_of(e);
    }

    return "<unknown>";
}
