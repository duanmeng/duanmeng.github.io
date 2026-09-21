#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>

// Teaching example. This byte flag uses 1 = null, unlike Velox's null bitmap.
template <bool kHasNulls, bool kAllRowsActive>
void squareRootKernel(
    const int32_t* __restrict positions,
    size_t count,
    const double* __restrict input,
    const uint8_t* __restrict isNull,
    double* __restrict output) {
  for (size_t i = 0; i < count; ++i) {
    const size_t row = kAllRowsActive ? i : positions[i];
    if constexpr (kHasNulls) {
      if (isNull[row]) {
        continue;
      }
    }
    output[row] = std::sqrt(input[row]);
  }
}

// Keep an externally visible loop for assembly inspection.
extern "C" void squareRootAll(
    const double* __restrict input,
    double* __restrict output,
    size_t count) {
  squareRootKernel<false, true>(nullptr, count, input, nullptr, output);
}

int main() {
  constexpr std::array<int32_t, 5> positions{0, 2, 5, 6, 7};
  constexpr std::array<uint8_t, 8> nulls{1, 0, 1, 0, 1, 0, 1, 0};
  constexpr std::array<double, 8> input{0, 1, 4, 8, 0, 1, 4, 8};
  std::array<double, 8> output;
  output.fill(-1);
  squareRootKernel<true, false>(
      positions.data(), positions.size(), input.data(), nulls.data(), output.data());
  for (size_t i = 0; i < input.size(); ++i) {
    const double expected = i == 5 || i == 7 ? std::sqrt(input[i]) : -1;
    assert(output[i] == expected);
  }
  squareRootAll(input.data(), output.data(), input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    assert(output[i] == std::sqrt(input[i]));
  }
  std::array<double, 259> largeInput, largeOutput;
  for (size_t i = 0; i < largeInput.size(); ++i) {
    largeInput[i] = double(i) / 7.0;
  }
  squareRootAll(largeInput.data(), largeOutput.data(), largeInput.size());
  for (size_t i = 0; i < largeInput.size(); ++i) {
    assert(largeOutput[i] == std::sqrt(largeInput[i]));
  }
  std::cout << "passed: sparse nulls, contiguous rows, and 259-row tail\n";
}
