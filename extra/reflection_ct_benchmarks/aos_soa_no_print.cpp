#ifdef USE_MODULES
import std;
#elif USE_PCH
#include "pch_meta_ranges.hpp"
#else
#include <meta>
#include <ranges>
#include <utility>
#endif

consteval auto nsdms(std::meta::info type) -> std::vector<std::meta::info> {
  return nonstatic_data_members_of(type, std::meta::access_context::current());
}

template <auto... V> struct replicator_type {
  template <typename F>
  constexpr auto operator>>(F body) const -> decltype(auto) {
    return body.template operator()<V...>();
  }
};

template <auto... V> replicator_type<V...> replicator = {};

consteval auto expand_all(std::span<std::meta::info const> r)
    -> std::meta::info {
  std::vector<std::meta::info> rv;
  for (std::meta::info i : r) {
    rv.push_back(reflect_constant(i));
  }
  return substitute(^^replicator, rv);
}

template <typename T>
consteval auto has_annotation(std::meta::info r, T const &value) -> bool {
  auto expected = std::meta::reflect_constant(value);
  for (std::meta::info a : annotations_of_with_type(r, ^^T)) {
    if (constant_of(a) == expected) {
      return true;
    }
  }
  return false;
}

template <class F> consteval auto transform_members(std::meta::info type, F f) {
  return nsdms(type) | std::views::transform([=](std::meta::info member) {
           return data_member_spec(f(type_of(member)),
                                   {.name = identifier_of(member)});
         });
}

template <class T> struct SoaVector {
private:
  struct Pointers;
  struct RefBase;
  consteval {
    define_aggregate(^^Pointers,
                     transform_members(^^T, std::meta::add_pointer));
    define_aggregate(^^RefBase,
                     transform_members(^^T, std::meta::add_lvalue_reference));
  }
  Pointers pointers_ = {};
  std::size_t size_ = 0;
  std::size_t capacity_ = 0;

  static constexpr auto mems = define_static_array(nsdms(^^T));
  static constexpr auto ptr_mems = define_static_array(nsdms(^^Pointers));
  static constexpr auto ref_mems = define_static_array(nsdms(^^RefBase));

  struct Ref : RefBase {
    auto operator=(T const &value) const -> void {
      template for (constexpr auto I : std::views::iota(0zu, mems.size())) {
        this->[:ref_mems[I]:] = value.[:mems[I]:];
      }
    }

    operator T() const {
      return [:expand_all(ref_mems):] >> [this]<auto... M> {
        return T{this->[:M:]...};
      };
    }
  };

  auto grow(std::size_t new_capacity) -> void {
    Pointers new_pointers = {};
    template for (constexpr auto M : ptr_mems) {
      new_pointers.[:M:] = allocate<typename[:remove_pointer(type_of(M)):]>(
                             new_capacity);
      std::uninitialized_copy_n(pointers_.[:M:], size_, new_pointers.[:M:]);
      delete_range(pointers_.[:M:]);
    }
    pointers_ = new_pointers;
    capacity_ = new_capacity;
  }

  template <class U> auto allocate(std::size_t cap) -> U * {
    return std::allocator<U>().allocate(cap);
  }

  template <class U> auto delete_range(U *ptr) -> void {
    std::destroy(ptr, ptr + size_);
    std::allocator<U>().deallocate(ptr, capacity_);
  }

public:
  SoaVector() = default;
  ~SoaVector() {
    template for (constexpr auto M : ptr_mems) {
      delete_range(pointers_.[:M:]);
    }
  }

  auto push_back(T const &value) -> void {
    if (size_ == capacity_) {
      grow(std::max(3 * size_ / 2, size_ + 2));
    }

    template for (constexpr auto I : std::views::iota(0zu, mems.size())) {
      constexpr auto from = mems[I];
      constexpr auto to = ptr_mems[I];

      using M = [:type_of(from):];
      ::new (pointers_.[:to:] + size_) M(value.[:from:]);
    }

    ++size_;
  }

  auto size() const -> std::size_t { return size_; }

  auto operator[](std::size_t idx) -> Ref {
    return [:expand_all(ptr_mems):] >> [this, idx]<auto... M> {
      return Ref{pointers_.[:M:][idx]...};
    };
  }

  auto operator[](std::size_t idx) const -> T {
    return [:expand_all(ptr_mems):] >> [this, idx]<auto... M> {
      return T{pointers_.[:M:][idx]...};
    };
  }
};

struct Point {
  char x;
  int y;
};

int main() {
  SoaVector<Point> v;
  v.push_back(Point{.x = 'e', .y = 4});
  v.push_back(Point{.x = 'f', .y = 7});

  v[0] = Point{.x = 'a', .y = 8};
}
