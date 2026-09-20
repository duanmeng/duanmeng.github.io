// Velox Type System article examples, checked against commit 1d1b765678702e.
// Compile-only interface and template checks. This file is not a query runner.
#include <cstdint>
#include <iostream>
#include <type_traits>
#include "velox/functions/Udf.h"
#include "velox/expression/UdfTypeResolver.h"
#include "velox/type/CppToType.h"
#include "velox/type/SimpleFunctionApi.h"
#include "velox/type/Type.h"
#include "velox/vector/BaseVector.h"
#include "velox/vector/FlatVector.h"

namespace facebook::velox::type_system_article {

static_assert(CppToType<int64_t>::typeKind == TypeKind::BIGINT);
static_assert(std::is_same_v<TypeTraits<TypeKind::BIGINT>::NativeType, int64_t>);
static_assert(std::is_same_v<TypeTraits<TypeKind::ARRAY>::NativeType, void>);
static_assert(std::is_same_v<TypeTraits<TypeKind::VARCHAR>::NativeType,
                             TypeTraits<TypeKind::VARBINARY>::NativeType>);
static_assert(std::is_same_v<exec::VectorExec::resolver<Date>::in_type, int32_t>);
static_assert(std::is_same_v<exec::VectorExec::resolver<Date>::out_type, int32_t>);

template <typename TExec>
struct IdentityDate {
  VELOX_DEFINE_FUNCTION_TYPES(TExec);
  bool call(out_type<Date>& out, const arg_type<Date>& in) {
    out = in;
    return true;
  }
};

void registerExample() {
  registerFunction<IdentityDate, Date, Date>({"identity_date"});
}

RowTypePtr makeSchema() {
  auto schema = ROW(
      {"user_id", "events", "attrs"},
      {BIGINT(),
       ARRAY(ROW({"ts", "cost"}, {TIMESTAMP(), DECIMAL(12, 2)})),
       MAP(VARCHAR(), VARCHAR())});
  return schema;
}

void createExamples(memory::MemoryPool* pool) {
  auto id = BIGINT();
  auto amount = DECIMAL(18, 4);
  auto datesType = ARRAY(DATE());
  auto attributes = MAP(VARCHAR(), VARBINARY());
  auto row = ROW({"id", "amount"}, {id, amount});
  auto cppType = CppToType<int64_t>::create();
  auto tagPhysicalType = CppToType<Date>::create();
  auto arrayTagPhysicalType = CppToType<Array<Date>>::create();
  auto arrayCppType = CppToType<Array<int64_t>>::create();
  auto factoryType = TypeFactory<TypeKind::BIGINT>::create();
  auto bigIntVT = TypeFactory<TypeKind::BIGINT>::create();
  auto arrayVT = TypeFactory<TypeKind::ARRAY>::create(bigIntVT);
  std::cout << bigIntVT->toString() << '\n';
  std::cout << arrayVT->toString() << '\n';
  auto dates = BaseVector::create(DATE(), 3, pool);
  auto amounts = BaseVector::create(DECIMAL(12, 2), 3, pool);
  auto* values = amounts->asFlatVector<int64_t>();
  values->set(0, 12345);
}

} // namespace facebook::velox::type_system_article
