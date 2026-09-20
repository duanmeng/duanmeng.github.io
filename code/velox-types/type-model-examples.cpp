// Type model observations against Velox commit 1d1b765678702e.
// Compile-only validation; runtime assertions have not been executed.
#include <cassert>
#include <cstdint>
#include <string>
#include <type_traits>
#include "velox/type/Type.h"
#include "velox/core/SimpleFunctionMetadata.h"

namespace facebook::velox::type_model_article {
static_assert(std::is_same_v<
    IntegerType, ScalarType<TypeKind::INTEGER>>);
static_assert(std::is_base_of_v<IntegerType, DateType>);
static_assert(std::is_same_v<
    TypeTraits<TypeKind::INTEGER>::NativeType, int32_t>);

void inspectTypeModel() {
  const TypePtr date = DATE();
  assert(date->kind() == TypeKind::INTEGER);
  assert(std::string(date->name()) == "DATE");
  assert(std::string(date->kindName()) == "INTEGER");
  assert(!date->equivalent(*INTEGER()));

  // C++ 基类引用不会抹去动态对象的逻辑身份。
  const IntegerType& asInteger = date->as<TypeKind::INTEGER>();
  assert(std::string(asInteger.name()) == "DATE");

  core::TypeAnalysisResults analysis;
  core::TypeAnalysis<Array<Date>>{}.run(analysis);
  assert(analysis.typeAsString() == "array(date)");
  assert(analysis.physicalType->equivalent(*ARRAY(INTEGER())));

  const auto logical = ARRAY(DATE());
  assert(logical->kindEquals(analysis.physicalType));
  assert(!logical->equivalent(*analysis.physicalType));
}

} // namespace facebook::velox::type_model_article
