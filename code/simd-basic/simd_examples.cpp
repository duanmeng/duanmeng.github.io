// C++17, GCC or Clang on x86-64. Build the translation unit for a baseline ISA.
// AVX/AVX2/FMA are enabled only on the functions carrying a target attribute.
#include <immintrin.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>
#if defined(__unix__)
#include <sys/mman.h>
#include <unistd.h>
#endif

#if !defined(__x86_64__) || (!defined(__GNUC__) && !defined(__clang__))
#error This example targets x86-64 GCC/Clang.
#endif

#define TARGET_AVX __attribute__((target("avx"), noinline))
#define TARGET_AVX2 __attribute__((target("avx2"), noinline))
#define TARGET_FMA __attribute__((target("avx,fma"), noinline))

void require(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}

// BEGIN add
TARGET_AVX
void add_f32_avx(const float* a, const float* b, float* out, std::size_t n) {
  std::size_t i = 0;
  for (; n - i >= 8; i += 8) {
    const __m256 x = _mm256_loadu_ps(a + i);
    const __m256 y = _mm256_loadu_ps(b + i);
    _mm256_storeu_ps(out + i, _mm256_add_ps(x, y));
  }
  for (; i < n; ++i) out[i] = a[i] + b[i];
}
// END add

void add_f32_scalar(const float* a, const float* b, float* out, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) out[i] = a[i] + b[i];
}

// BEGIN count
TARGET_AVX2
std::size_t count_gt_u32_avx2(
    const std::uint32_t* data, std::size_t n, std::uint32_t threshold) {
  const __m256i sign = _mm256_set1_epi32(std::numeric_limits<int>::min());
  std::int32_t thresholdBits;
  std::memcpy(&thresholdBits, &threshold, sizeof(thresholdBits));
  const __m256i limit = _mm256_xor_si256(_mm256_set1_epi32(thresholdBits), sign);
  std::size_t count = 0, i = 0;
  for (; n - i >= 8; i += 8) {
    const __m256i x = _mm256_loadu_si256(
        reinterpret_cast<const __m256i*>(data + i));
    const __m256i mask = _mm256_cmpgt_epi32(_mm256_xor_si256(x, sign), limit);
    unsigned bits = static_cast<unsigned>(
        _mm256_movemask_ps(_mm256_castsi256_ps(mask)));
    while (bits != 0) {
      bits &= bits - 1;
      ++count;
    }
  }
  for (; i < n; ++i) count += data[i] > threshold;
  return count;
}
// END count

std::size_t count_gt_u32_scalar(
    const std::uint32_t* data, std::size_t n, std::uint32_t threshold) {
  std::size_t count = 0;
  for (std::size_t i = 0; i < n; ++i) count += data[i] > threshold;
  return count;
}

// BEGIN dispatch
std::size_t count_gt_u32(
    const std::uint32_t* data, std::size_t n, std::uint32_t threshold) {
  using Fn = std::size_t (*)(const std::uint32_t*, std::size_t, std::uint32_t);
  static const Fn fn = __builtin_cpu_supports("avx2")
      ? count_gt_u32_avx2
      : count_gt_u32_scalar;
  return fn(data, n, threshold);
}
// END dispatch

// BEGIN reduce
TARGET_AVX
float sum_f32_avx(const float* data, std::size_t n) {
  __m256 acc0 = _mm256_setzero_ps(), acc1 = _mm256_setzero_ps();
  std::size_t i = 0;
  for (; n - i >= 16; i += 16) {
    acc0 = _mm256_add_ps(acc0, _mm256_loadu_ps(data + i));
    acc1 = _mm256_add_ps(acc1, _mm256_loadu_ps(data + i + 8));
  }
  __m256 v = _mm256_add_ps(acc0, acc1);
  if (n - i >= 8) {
    v = _mm256_add_ps(v, _mm256_loadu_ps(data + i));
    i += 8;
  }
  __m128 s = _mm_add_ps(_mm256_castps256_ps128(v), _mm256_extractf128_ps(v, 1));
  s = _mm_add_ps(s, _mm_movehl_ps(s, s));
  s = _mm_add_ss(s, _mm_shuffle_ps(s, s, _MM_SHUFFLE(1, 1, 1, 1)));
  float result = _mm_cvtss_f32(s);
  for (; i < n; ++i) result += data[i];
  return result;
}
// END reduce

// BEGIN masked_tail
TARGET_AVX2
void load_tail_u32(const std::uint32_t* input, int count, std::uint32_t* out8) {
  // Contract: 0 <= count <= 8, input has count readable elements;
  // out8 always has eight writable elements. Disabled lanes become zero.
  const __m256i index = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
  const __m256i mask = _mm256_cmpgt_epi32(_mm256_set1_epi32(count), index);
  const __m256i x = _mm256_maskload_epi32(reinterpret_cast<const int*>(input), mask);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(out8), x);
}
// END masked_tail

TARGET_FMA
void fma_f32(const float* a, const float* b, const float* c, float* out, std::size_t n) {
  std::size_t i = 0;
  for (; n - i >= 8; i += 8) {
    const __m256 x = _mm256_loadu_ps(a + i), y = _mm256_loadu_ps(b + i);
    _mm256_storeu_ps(out + i, _mm256_fmadd_ps(x, y, _mm256_loadu_ps(c + i)));
  }
  for (; i < n; ++i) out[i] = std::fma(a[i], b[i], c[i]);
}

TARGET_AVX
void test_avx_semantics() {
  alignas(32) float x[8], out[8];
  _mm256_store_ps(x, _mm256_set_ps(7, 6, 5, 4, 3, 2, 1, 0));
  for (int i = 0; i < 8; ++i) require(x[i] == i, "set argument order");
  __m256 v = _mm256_load_ps(x);
  _mm256_store_ps(out, _mm256_permute_ps(v, _MM_SHUFFLE(0, 1, 2, 3)));
  const float expected[] = {3, 2, 1, 0, 7, 6, 5, 4};
  require(std::equal(out, out + 8, expected), "permute lane boundary");
  const __m256 b = _mm256_add_ps(v, _mm256_set1_ps(10));
  _mm256_store_ps(out, _mm256_shuffle_ps(v, b, _MM_SHUFFLE(3, 2, 1, 0)));
  const float mixed[] = {0, 1, 12, 13, 4, 5, 16, 17};
  require(std::equal(out, out + 8, mixed), "shuffle two inputs");
  const __m256 nan = _mm256_set1_ps(std::numeric_limits<float>::quiet_NaN());
  require(_mm256_movemask_ps(_mm256_cmp_ps(nan, v, _CMP_GT_OQ)) == 0,
          "ordered NaN comparison");
}

TARGET_AVX2
void test_avx2_semantics() {
  alignas(32) std::int32_t a[8] = {-3, 100, 5, 200, -7, 300, 11, 400};
  alignas(32) std::int32_t b[8] = {2, 2, 3, 3, 4, 4, 5, 5};
  alignas(32) std::int64_t products[4];
  const __m256i va = _mm256_load_si256(reinterpret_cast<const __m256i*>(a));
  const __m256i vb = _mm256_load_si256(reinterpret_cast<const __m256i*>(b));
  _mm256_store_si256(reinterpret_cast<__m256i*>(products), _mm256_mul_epi32(va, vb));
  for (int i = 0; i < 4; ++i)
    require(products[i] == std::int64_t(a[2 * i]) * b[2 * i], "even-lane widening multiply");

  alignas(32) std::uint8_t bytes[32], indices[32], shuffled[32];
  for (int i = 0; i < 32; ++i) { bytes[i] = i; indices[i] = 31 - i; }
  indices[3] = 0x80; indices[20] = 0x7f;
  _mm256_store_si256(reinterpret_cast<__m256i*>(shuffled), _mm256_shuffle_epi8(
      _mm256_load_si256(reinterpret_cast<const __m256i*>(bytes)),
      _mm256_load_si256(reinterpret_cast<const __m256i*>(indices))));
  for (int i = 0; i < 32; ++i) {
    const int expected = (indices[i] & 0x80) ? 0 : (i / 16) * 16 + (indices[i] & 15);
    require(shuffled[i] == expected, "byte shuffle mask semantics");
  }

  alignas(32) int laneIds[8] = {0, 1, 2, 3, 4, 5, 6, 7}, reversed[8];
  const __m256i ids = _mm256_load_si256(reinterpret_cast<const __m256i*>(laneIds));
  const __m256i reverse = _mm256_setr_epi32(7, 6, 5, 4, 3, 2, 1, 0);
  _mm256_store_si256(reinterpret_cast<__m256i*>(reversed), _mm256_permutevar8x32_epi32(ids, reverse));
  for (int i = 0; i < 8; ++i) require(reversed[i] == 7 - i, "cross-lane permute");

  alignas(32) int gatherIndices[8] = {7, 0, 6, 1, 5, 2, 4, 3}, gathered[8];
  _mm256_store_si256(reinterpret_cast<__m256i*>(gathered), _mm256_i32gather_epi32(
      laneIds, _mm256_load_si256(reinterpret_cast<const __m256i*>(gatherIndices)), 4));
  for (int i = 0; i < 8; ++i) require(gathered[i] == gatherIndices[i], "gather byte scale");

  alignas(32) int shifted[8];
  const __m256i counts = _mm256_setr_epi32(0, 1, 31, 32, 33, 63, 64, -1);
  _mm256_store_si256(reinterpret_cast<__m256i*>(shifted),
      _mm256_sllv_epi32(_mm256_set1_epi32(1), counts));
  require(shifted[0] == 1 && shifted[1] == 2 && shifted[2] == std::numeric_limits<int>::min(),
          "variable shift within width");
  for (int i = 3; i < 8; ++i) require(shifted[i] == 0, "variable shift >= width");
}

int main() {
  try {
    const bool avx = __builtin_cpu_supports("avx");
    const bool avx2 = __builtin_cpu_supports("avx2");
    const bool fma = avx && __builtin_cpu_supports("fma");
    std::mt19937 rng(20260919);
    std::size_t cases = 0;
    for (std::size_t n = 0; n <= 257; ++n) {
      // Offset by one float/u32 to exercise addresses not guaranteed 32-byte aligned.
      std::vector<std::uint32_t> values(n + 1);
      std::vector<float> a(n + 1), b(n + 1), out(n + 1);
      for (std::size_t i = 0; i < n; ++i) {
        values[i + 1] = rng();
        a[i + 1] = float(int(rng() % 201) - 100);
        b[i + 1] = float(int(rng() % 201) - 100);
      }
      for (auto threshold : {0u, 1u, 0x7fffffffu, 0x80000000u, 0xfffffffeu, 0xffffffffu}) {
        const auto expected = count_gt_u32_scalar(values.data() + 1, n, threshold);
        require(count_gt_u32(values.data() + 1, n, threshold) == expected, "dispatched count");
        if (avx2) require(count_gt_u32_avx2(values.data() + 1, n, threshold) == expected, "AVX2 count");
        ++cases;
      }
      if (avx) {
        add_f32_avx(a.data() + 1, b.data() + 1, out.data() + 1, n);
        float exactSum = 0;
        for (std::size_t i = 0; i < n; ++i) {
          require(out[i + 1] == a[i + 1] + b[i + 1], "addition/tail");
          exactSum += a[i + 1];
        }
        // These small integral float values sum exactly; this is not a general
        // promise that vector and scalar floating-point reductions are bit-equal.
        require(sum_f32_avx(a.data() + 1, n) == exactSum, "reduction/tail");
      }
    }
    if (avx) test_avx_semantics();
    if (avx2) {
      test_avx2_semantics();
      const std::uint32_t edge[] = {0, 1, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff, 0, 0xffffffff};
      for (auto t : edge) require(count_gt_u32_avx2(edge, 8, t) == count_gt_u32_scalar(edge, 8, t),
                                 "unsigned equality and sign-boundary values");
      for (int n = 0; n <= 8; ++n) {
        // Exact-sized allocation: enabled lanes are in-bounds, output has 8 slots.
        std::vector<std::uint32_t> x(std::max(n, 1), 123);
        std::array<std::uint32_t, 8> out{};
        load_tail_u32(x.data(), n, out.data());
        for (int j = 0; j < 8; ++j) require(out[j] == (j < n ? 123u : 0u), "masked tail");
      }
#if defined(__unix__)
      const long pageSize = sysconf(_SC_PAGESIZE);
      require(pageSize > 0, "page size");
      void* pages = mmap(nullptr, 2 * pageSize, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      require(pages != MAP_FAILED, "mmap");
      require(mprotect(static_cast<char*>(pages) + pageSize, pageSize, PROT_NONE) == 0, "mprotect");
      auto* end = reinterpret_cast<std::uint32_t*>(static_cast<char*>(pages) + pageSize);
      for (int n = 0; n <= 8; ++n) {
        std::uint32_t* tail = end - n;
        for (int j = 0; j < n; ++j) tail[j] = 123;
        std::array<std::uint32_t, 8> out{};
        load_tail_u32(tail, n, out.data());
        require(count_gt_u32_avx2(tail, n, 100) == std::size_t(n), "count at guard page");
        for (int j = 0; j < 8; ++j) require(out[j] == (j < n ? 123u : 0u), "masked load at guard page");
      }
      require(munmap(pages, 2 * pageSize) == 0, "munmap");
#endif
    }
    if (fma) {
      const float epsilon = std::ldexp(1.0f, -23);
      std::vector<float> a(17, 1.0f + epsilon), b(17, 1.0f - epsilon), c(17, -1), out(17);
      fma_f32(a.data(), b.data(), c.data(), out.data(), out.size());
      for (float v : out) require(v == -std::ldexp(1.0f, -46), "FMA single rounding including tail");
    }
    std::cout << "PASS: " << cases << " count cases; lengths 0..257; AVX=" << avx
              << " AVX2=" << avx2 << " FMA=" << fma << "; lane, tail, mask, shuffle and reduction checks\n";
  } catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
