// Copyright 2012 Emilie Gillet.

#ifndef STMLIB_STMLIB_H_
#define STMLIB_STMLIB_H_

#include <inttypes.h>
#include <stddef.h>

#ifndef NULL
#define NULL 0
#endif

#ifdef _MSC_VER
  #define forcedinline __forceinline
#else
  #define forcedinline __attribute__((always_inline))
#endif

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

#define CLIP(x) if (x < -32767) x = -32767; if (x > 32767) x = 32767;

#define CONSTRAIN(var, min, max) \
  if (var < (min)) { \
    var = (min); \
  } else if (var > (max)) { \
    var = (max); \
  }

#define JOIN(lhs, rhs)    JOIN_1(lhs, rhs)
#define JOIN_1(lhs, rhs)  JOIN_2(lhs, rhs)
#define JOIN_2(lhs, rhs)  lhs##rhs

#define STATIC_ASSERT(expression, message)\
  struct JOIN(__static_assertion_at_line_, __LINE__)\
  {\
    impl::StaticAssertion<static_cast<bool>((expression))> JOIN(JOIN(JOIN(STATIC_ASSERTION_FAILED_AT_LINE_, __LINE__), _), message);\
  };\
  typedef impl::StaticAssertionTest<sizeof(JOIN(__static_assertion_at_line_, __LINE__))> JOIN(__static_assertion_test_at_line_, __LINE__)

namespace impl {

  template <bool>
  struct StaticAssertion;

  template <>
  struct StaticAssertion<true>
  {
  }; // StaticAssertion<true>

  template<int i>
  struct StaticAssertionTest
  {
  }; // StaticAssertionTest<int>

} // namespace impl


#ifndef TEST
#define IN_RAM __attribute__ ((section (".ramtext")))
#else
#define IN_RAM
#endif  // TEST

#define UNROLL2(x) x; x;
#define UNROLL4(x) x; x; x; x;
#define UNROLL8(x) x; x; x; x; x; x; x; x;

template<bool b>
inline void StaticAssertImplementation() {
	char static_assert_size_mismatch[b] = { 0 };
}
 
namespace stmlib {

typedef union {
  uint16_t value;
  uint8_t bytes[2];
} Word;

typedef union {
  uint32_t value;
  uint16_t words[2];
  uint8_t bytes[4];
} LongWord;

}  // namespace stmlib

#endif   // STMLIB_STMLIB_H_
