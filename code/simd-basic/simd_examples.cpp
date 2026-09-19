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

// BEGIN saxpy
TARGET_AVX
void saxpy_avx(float alpha, const float* x, const float* y,
               float* out, std::size_t n) {
  const __m256 a = _mm256_set1_ps(alpha);
  std::size_t i = 0;
  for (; n - i >= 8; i += 8) {
    const __m256 vx = _mm256_loadu_ps(x + i);
    const __m256 vy = _mm256_loadu_ps(y + i);
    const __m256 product = _mm256_mul_ps(a, vx);
    _mm256_storeu_ps(out + i, _mm256_add_ps(product, vy));
  }
  for (; i < n; ++i) out[i] = alpha * x[i] + y[i];
}
// END saxpy

// BEGIN signed_count
TARGET_AVX2
std::size_t count_gt_i32_avx2(const std::int32_t* data, std::size_t n,
                            std::int32_t threshold) {
  const __m256i limit = _mm256_set1_epi32(threshold);
  std::size_t count = 0, i = 0;
  for (; n - i >= 8; i += 8) {
    const __m256i x = _mm256_loadu_si256(
        reinterpret_cast<const __m256i*>(data + i));
    const __m256i mask = _mm256_cmpgt_epi32(x, limit);
    unsigned bits = static_cast<unsigned>(
        _mm256_movemask_ps(_mm256_castsi256_ps(mask)));
    // Portable popcount: each iteration clears one set bit.
    while (bits) { bits &= bits - 1; ++count; }
  }
  for (; i < n; ++i) count += data[i] > threshold;
  return count;
}
// END signed_count

// BEGIN ascii_upper
TARGET_AVX2
void ascii_upper_avx2(std::uint8_t* bytes, std::size_t n) {
  const __m256i beforeA = _mm256_set1_epi8('a' - 1);
  const __m256i afterZ = _mm256_set1_epi8('z' + 1);
  const __m256i caseBit = _mm256_set1_epi8(0x20);
  std::size_t i = 0;
  for (; n - i >= 32; i += 32) {
    const __m256i x = _mm256_loadu_si256(
        reinterpret_cast<const __m256i*>(bytes + i));
    const __m256i lower = _mm256_cmpgt_epi8(x, beforeA);
    const __m256i upper = _mm256_cmpgt_epi8(afterZ, x);
    const __m256i mask = _mm256_and_si256(lower, upper);
    const __m256i delta = _mm256_and_si256(mask, caseBit);
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(bytes + i),
                       _mm256_sub_epi8(x, delta));
  }
  for (; i < n; ++i)
    if (bytes[i] >= 'a' && bytes[i] <= 'z') bytes[i] -= 0x20;
}
// END ascii_upper

// BEGIN deinterleave
TARGET_AVX2
void deinterleave_xy_avx2(const float* xy, float* x, float* y,
                         std::size_t n) {
  // xy holds 2*n floats; x and y each hold n floats. Buffers are disjoint.
  const __m256i order = _mm256_setr_epi32(0, 1, 4, 5, 2, 3, 6, 7);
  std::size_t i = 0;
  for (; n - i >= 8; i += 8) {
    const __m256 lo = _mm256_loadu_ps(xy + 2 * i);
    const __m256 hi = _mm256_loadu_ps(xy + 2 * i + 8);
    const __m256 tx = _mm256_shuffle_ps(lo, hi, _MM_SHUFFLE(2, 0, 2, 0));
    const __m256 ty = _mm256_shuffle_ps(lo, hi, _MM_SHUFFLE(3, 1, 3, 1));
    _mm256_storeu_ps(x + i, _mm256_permutevar8x32_ps(tx, order));
    _mm256_storeu_ps(y + i, _mm256_permutevar8x32_ps(ty, order));
  }
  for (; i < n; ++i) { x[i] = xy[2 * i]; y[i] = xy[2 * i + 1]; }
}
// END deinterleave

// BEGIN horizontal_sum
TARGET_AVX
float horizontal_sum8_avx(__m256 v) {
  __m128 s = _mm_add_ps(_mm256_castps256_ps128(v),
                        _mm256_extractf128_ps(v, 1));
  s = _mm_add_ps(s, _mm_movehl_ps(s, s));
  s = _mm_add_ss(s, _mm_shuffle_ps(s, s, _MM_SHUFFLE(1, 1, 1, 1)));
  return _mm_cvtss_f32(s);
}
// END horizontal_sum

// BEGIN dot
TARGET_AVX
float dot_f32_avx(const float* x, const float* y, std::size_t n) {
  __m256 acc0 = _mm256_setzero_ps(), acc1 = _mm256_setzero_ps();
  std::size_t i = 0;
  for (; n - i >= 16; i += 16) {
    const __m256 p0 = _mm256_mul_ps(_mm256_loadu_ps(x + i),
                                   _mm256_loadu_ps(y + i));
    const __m256 p1 = _mm256_mul_ps(_mm256_loadu_ps(x + i + 8),
                                   _mm256_loadu_ps(y + i + 8));
    acc0 = _mm256_add_ps(acc0, p0);
    acc1 = _mm256_add_ps(acc1, p1);
  }
  __m256 sum = _mm256_add_ps(acc0, acc1);
  if (n - i >= 8) {
    sum = _mm256_add_ps(sum, _mm256_mul_ps(_mm256_loadu_ps(x + i),
                                         _mm256_loadu_ps(y + i)));
    i += 8;
  }
  float result = horizontal_sum8_avx(sum);
  for (; i < n; ++i) result += x[i] * y[i];
  return result;
}
// END dot

// BEGIN byte_popcount
TARGET_AVX2
std::uint64_t popcount_bytes_avx2(const std::uint8_t* data, std::size_t n) {
  const __m256i lut = _mm256_setr_epi8(
      0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
      0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4);
  const __m256i nibble = _mm256_set1_epi8(0x0f);
  const __m256i zero = _mm256_setzero_si256();
  std::uint64_t count = 0;
  std::size_t i = 0;
  for (; n - i >= 32; i += 32) {
    const __m256i x = _mm256_loadu_si256(
        reinterpret_cast<const __m256i*>(data + i));
    const __m256i lo = _mm256_and_si256(x, nibble);
    const __m256i hi = _mm256_and_si256(_mm256_srli_epi16(x, 4), nibble);
    const __m256i eachByte = _mm256_add_epi8(
        _mm256_shuffle_epi8(lut, lo), _mm256_shuffle_epi8(lut, hi));
    const __m256i groups = _mm256_sad_epu8(eachByte, zero);
    alignas(32) std::uint64_t sums[4];
    _mm256_store_si256(reinterpret_cast<__m256i*>(sums), groups);
    count += sums[0] + sums[1] + sums[2] + sums[3];
  }
  for (; i < n; ++i) {
    unsigned x = data[i];
    while (x) { x &= x - 1; ++count; }
  }
  return count;
}
// END byte_popcount

// BEGIN brighten
TARGET_AVX2
void brighten_u8_avx2(std::uint8_t* data, std::size_t n, std::uint8_t delta) {
  char deltaBits;
  std::memcpy(&deltaBits, &delta, sizeof(deltaBits));
  const __m256i amount = _mm256_set1_epi8(deltaBits);
  std::size_t i = 0;
  for (; n - i >= 32; i += 32) {
    const __m256i x = _mm256_loadu_si256(
        reinterpret_cast<const __m256i*>(data + i));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i),
                       _mm256_adds_epu8(x, amount));
  }
  for (; i < n; ++i)
    data[i] = static_cast<std::uint8_t>(std::min(255u, unsigned(data[i]) + delta));
}
// END brighten

TARGET_AVX
void test_tutorial_avx() {
  alignas(32) float out[8];
  const __m256 a = _mm256_setr_ps(0,1,2,3,4,5,6,7);
  const __m256 b = _mm256_setr_ps(10,11,12,13,14,15,16,17);
  _mm256_store_ps(out, _mm256_shuffle_ps(a,b,_MM_SHUFFLE(2,0,2,0)));
  const float expected[] = {0,2,10,12,4,6,14,16};
  require(std::equal(out,out+8,expected), "two-source shuffle diagram");
  _mm256_store_ps(out, _mm256_permute2f128_ps(a,a,0x01));
  const float swapped[] = {4,5,6,7,0,1,2,3};
  require(std::equal(out,out+8,swapped), "128-bit block swap diagram");
  require(horizontal_sum8_avx(_mm256_setr_ps(1,2,3,4,5,6,7,8))==36,
          "full reduction diagram");
  const __m256 v = _mm256_setr_ps(-3,9,0,12,7,-1,20,4);
  const __m256 mask = _mm256_cmp_ps(v,_mm256_set1_ps(5),_CMP_GT_OQ);
  require(_mm256_movemask_ps(mask)==0x5a,"comparison mask diagram");
  _mm256_store_ps(out,_mm256_blendv_ps(_mm256_setzero_ps(),v,mask));
  const float selected[] = {0,9,0,12,7,0,20,0};
  require(std::equal(out,out+8,selected),"blend selection diagram");
  const __m256i partial = _mm256_setr_epi32(1,std::numeric_limits<int>::min(),-1,0,1,0,0,0);
  _mm256_store_ps(out,_mm256_blendv_ps(a,b,_mm256_castsi256_ps(partial)));
  const float signSelected[] = {0,11,12,3,4,5,6,7};
  require(std::equal(out,out+8,signSelected),"blendv sign bit, not any nonzero bit");
}

TARGET_AVX2
void test_tutorial_avx2() {
  alignas(32) std::int32_t out[8];
  const __m256i a = _mm256_setr_epi32(0,1,2,3,4,5,6,7);
  const __m256i b = _mm256_setr_epi32(10,11,12,13,14,15,16,17);
  _mm256_store_si256(reinterpret_cast<__m256i*>(out),_mm256_unpacklo_epi32(a,b));
  const int lo[] = {0,10,1,11,4,14,5,15};
  require(std::equal(out,out+8,lo),"unpacklo is local to each 128-bit half");
  _mm256_store_si256(reinterpret_cast<__m256i*>(out),_mm256_unpackhi_epi32(a,b));
  const int hi[] = {2,12,3,13,6,16,7,17};
  require(std::equal(out,out+8,hi),"unpackhi is local to each 128-bit half");
  const __m256i wideA=_mm256_setr_epi32(1,2,3,40000,5,6,7,-40000);
  const __m256i wideB=_mm256_setr_epi32(101,102,103,104,105,106,107,108);
  alignas(32) std::int16_t packed[16];
  _mm256_store_si256(reinterpret_cast<__m256i*>(packed),_mm256_packs_epi32(wideA,wideB));
  const std::int16_t wanted[]={1,2,3,32767,101,102,103,104,5,6,7,-32768,105,106,107,108};
  require(std::equal(packed,packed+16,wanted),"saturating pack order diagram");
  const __m256i values=_mm256_setr_epi32(-3,9,0,12,7,-1,20,4);
  const __m256i mask=_mm256_cmpgt_epi32(values,_mm256_set1_epi32(5));
  require(_mm256_movemask_ps(_mm256_castsi256_ps(mask))==0x5a,"dword movemask diagram");
  require(static_cast<unsigned>(_mm256_movemask_epi8(mask))==0x0f0ff0f0u,
          "byte movemask contains four bits per true dword");
  alignas(32) std::uint8_t bytes[32], shifted[32];
  for(int i=0;i<32;++i)bytes[i]=static_cast<std::uint8_t>(i+1);
  _mm256_store_si256(reinterpret_cast<__m256i*>(shifted),_mm256_slli_si256(
      _mm256_load_si256(reinterpret_cast<const __m256i*>(bytes)),4));
  for(int i=0;i<32;++i)require(shifted[i]==(i%16<4?0:bytes[i-4]),"byte shift does not cross128");
  const unsigned saved=_mm_getcsr();
  _MM_SET_ROUNDING_MODE(_MM_ROUND_NEAREST);
  const __m256 fractional=_mm256_setr_ps(-1.9f,-1.1f,-0.5f,0,0.5f,1.1f,1.9f,2.5f);
  _mm256_store_si256(reinterpret_cast<__m256i*>(out),_mm256_cvttps_epi32(fractional));
  const int truncated[]={-1,-1,0,0,0,1,1,2};
  require(std::equal(out,out+8,truncated),"truncate conversion example");
  _mm256_store_si256(reinterpret_cast<__m256i*>(out),_mm256_cvtps_epi32(fractional));
  const int rounded[]={-2,-1,0,0,0,1,2,2};
  require(std::equal(out,out+8,rounded),"nearest-even conversion example");
  _mm_setcsr(saved);
}

void test_expanded_examples(bool avx, bool avx2, bool fma) {
  std::mt19937 rng(20260920);
  std::size_t examples=0;
  for(std::size_t n=0;n<=513;++n){
    std::vector<float>x(n+1),y(n+1),out(n+1),xy(2*n+1),separateX(n+1),separateY(n+1);
    std::vector<std::int32_t> ints(n+1);
    std::vector<std::uint8_t> bytes(n+1),upper(n+1),bright(n+1);
    float exactDot=0;
    std::size_t expectedCount=0;
    std::uint64_t expectedBits=0;
    for(std::size_t i=0;i<n;++i){
      x[i+1]=float(int(rng()%127)-63);y[i+1]=float(int(rng()%63)-31);
      exactDot+=x[i+1]*y[i+1];ints[i+1]=int(rng()%201)-100;expectedCount+=ints[i+1]>5;
      bytes[i+1]=static_cast<std::uint8_t>(rng());
      for(int bit=0;bit<8;++bit)expectedBits+=(bytes[i+1]>>bit)&1u;
      xy[2*i+1]=float(i)+0.25f;xy[2*i+2]=-float(i)-0.5f;
    }
    if(avx){
      saxpy_avx(0.5f,x.data()+1,y.data()+1,out.data()+1,n);
      for(std::size_t i=0;i<n;++i)require(out[i+1]==0.5f*x[i+1]+y[i+1],"SAXPY reference and tail");
      require(dot_f32_avx(x.data()+1,y.data()+1,n)==exactDot,"dot product exact bounded integers");
      examples+=2;
    }
    if(avx2){
      require(count_gt_i32_avx2(ints.data()+1,n,5)==expectedCount,"signed count reference and tail");
      upper=bytes;ascii_upper_avx2(upper.data()+1,n);
      for(std::size_t i=0;i<n;++i){const auto v=bytes[i+1];require(upper[i+1]==(v>='a'&&v<='z'?v-0x20:v),"ASCII reference and tail");}
      require(popcount_bytes_avx2(bytes.data()+1,n)==expectedBits,"nibble lookup popcount reference");
      for(auto delta:{std::uint8_t(0),std::uint8_t(20),std::uint8_t(255)}){
        bright=bytes;brighten_u8_avx2(bright.data()+1,n,delta);
        for(std::size_t i=0;i<n;++i)require(bright[i+1]==std::min(255u,unsigned(bytes[i+1])+delta),"saturating brightness");
      }
      deinterleave_xy_avx2(xy.data()+1,separateX.data()+1,separateY.data()+1,n);
      for(std::size_t i=0;i<n;++i){require(separateX[i+1]==xy[2*i+1],"AoS/SoA x order");require(separateY[i+1]==xy[2*i+2],"AoS/SoA y order");}
      examples+=7;
    }
  }
  if(avx)test_tutorial_avx();
  if(avx2){
    test_tutorial_avx2();
    std::array<std::uint8_t,256> all{};
    for(int i=0;i<256;++i)all[i]=static_cast<std::uint8_t>(i);
    ascii_upper_avx2(all.data(),all.size());
    for(int i=0;i<256;++i)require(all[i]==(i>='a'&&i<='z'?i-0x20:i),"ASCII exhaustive byte domain");
    const std::int32_t edge[]={std::numeric_limits<int>::min(),-1,0,1,5,6,100,std::numeric_limits<int>::max()};
    for(auto t:edge){std::size_t expected=0;for(auto v:edge)expected+=v>t;require(count_gt_i32_avx2(edge,8,t)==expected,"signed count edge values");}
  }
  if(fma){
    const float epsilon=std::ldexp(1.0f,-13);
    std::array<float,8>a{},b{},c{},out{};
    a.fill(1+epsilon);b.fill(1-epsilon);c.fill(-1);
    fma_f32(a.data(),b.data(),c.data(),out.data(),8);
    for(float v:out)require(v==-std::ldexp(1.0f,-26),"FMA diagram single rounding");
    volatile float product=a[0]*b[0];
    require(product+c[0]==0,"separate multiply/add rounds twice");
  }
  std::cout<<"PASS: "<<examples<<" application cases; lengths 0..513; exact lane diagrams; ASCII all256 values\n";
}


int main() {
  try {
    const bool avx = __builtin_cpu_supports("avx");
    const bool avx2 = __builtin_cpu_supports("avx2");
    const bool fma = avx && __builtin_cpu_supports("fma");
    test_expanded_examples(avx, avx2, fma);
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
