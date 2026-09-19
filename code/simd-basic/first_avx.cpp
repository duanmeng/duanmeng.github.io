#include <immintrin.h>
#include <iostream>

// Build with -mavx and run only on a machine with usable AVX support.
int main() {
  const float a[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  const float b[8] = {10, 11, 12, 13, 14, 15, 16, 17};
  float out[8];
  const __m256 va = _mm256_loadu_ps(a);
  const __m256 vb = _mm256_loadu_ps(b);
  const __m256 sum = _mm256_add_ps(va, vb);
  _mm256_storeu_ps(out, sum);
  for (int i = 0; i < 8; ++i) std::cout << (i ? " " : "") << out[i];
  std::cout << '\n';
}
