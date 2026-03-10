#ifdef USE_MODULES
import std;
#elif USE_PCH
#include "pch_meta.hpp"
#else
#include <meta>
#include <string_view>
#endif

volatile int sink = 0;
void use(auto &&x) { sink += sizeof(x); }

template <typename T> void reflect_struct(const T &obj) {
  template for (constexpr std::meta::info field :
                std::define_static_array(std::meta::nonstatic_data_members_of(
                    ^^T, std::meta::access_context::current()))) {
    use(std::meta::identifier_of(field));
    use(obj.[:field:]);
  }
}

template <int> struct User {
  std::string_view name;
  int age;
  bool active;
};

int main() {
  reflect_struct(User<0>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<1>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<2>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<3>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<4>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<5>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<6>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<7>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<8>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<9>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<10>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<11>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<12>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<13>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<14>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<15>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<16>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<17>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<18>{.name = "Alice", .age = 30, .active = true});
  reflect_struct(User<19>{.name = "Alice", .age = 30, .active = true});
}
