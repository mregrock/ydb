#include "aggr_common.h"
#include "aggr_keys.h"
#include "custom_registry.h"

#include <ydb/library/arrow_kernels/func_common.h>
#include <ydb/library/arrow_kernels/functions.h>

#include <contrib/libs/apache/arrow/cpp/src/arrow/compute/api.h>
#include <contrib/libs/apache/arrow/cpp/src/arrow/compute/registry_internal.h>
#include <util/system/yassert.h>

#ifndef WIN32
#ifdef NO_SANITIZE_THREAD
#undef NO_SANITIZE_THREAD
#endif
#include <AggregateFunctions/AggregateFunctionAvg.h>
#include <AggregateFunctions/AggregateFunctionCount.h>
#include <AggregateFunctions/AggregateFunctionMinMaxAny.h>
#include <AggregateFunctions/AggregateFunctionNumRows.h>
#include <AggregateFunctions/AggregateFunctionSum.h>
#endif

namespace cp = ::arrow::compute;

using namespace NKikimr::NKernels;

namespace NKikimr::NArrow::NSSA {

static void RegisterMath(cp::FunctionRegistry* registry) {
    Y_ABORT_UNLESS(registry->AddFunction(MakeMathUnary<TAcosh>(TAcosh::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeMathUnary<TAtanh>(TAtanh::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeMathUnary<TCbrt>(TCbrt::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeMathUnary<TCosh>(TCosh::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeConstNullary<TE>(TE::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeMathUnary<TErf>(TErf::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeMathUnary<TErfc>(TErfc::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeMathUnary<TExp>(TExp::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeMathUnary<TExp2>(TExp2::Name)).ok());
    // Temporarily disabled because of compilation error on Windows.
#if 0
    Y_ABORT_UNLESS(registry->AddFunction(MakeMathUnary<TExp10>(TExp10::Name)).ok());
#endif
    Y_ABORT_UNLESS(registry->AddFunction(MakeMathBinary<THypot>(THypot::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeMathUnary<TLgamma>(TLgamma::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeConstNullary<TPi>(TPi::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeMathUnary<TSinh>(TSinh::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeMathUnary<TSqrt>(TSqrt::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeMathUnary<TTgamma>(TTgamma::Name)).ok());
}

static void RegisterRound(cp::FunctionRegistry* registry) {
    Y_ABORT_UNLESS(registry->AddFunction(MakeArithmeticUnary<TRound>(TRound::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeArithmeticUnary<TRoundBankers>(TRoundBankers::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeArithmeticUnary<TRoundToExp2>(TRoundToExp2::Name)).ok());
}

static void RegisterArithmetic(cp::FunctionRegistry* registry) {
    Y_ABORT_UNLESS(registry->AddFunction(MakeArithmeticIntBinary<TGreatestCommonDivisor>(TGreatestCommonDivisor::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeArithmeticIntBinary<TLeastCommonMultiple>(TLeastCommonMultiple::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeArithmeticBinary<TModulo>(TModulo::Name)).ok());
    Y_ABORT_UNLESS(registry->AddFunction(MakeArithmeticBinary<TModuloOrZero>(TModuloOrZero::Name)).ok());
}

static void RegisterYdbCast(cp::FunctionRegistry* registry) {
    cp::internal::RegisterScalarCast(registry);
    Y_ABORT_UNLESS(registry->AddFunction(std::make_shared<YdbCastMetaFunction>()).ok());
}

static void RegisterCustomAggregates(cp::FunctionRegistry* registry) {
    Y_ABORT_UNLESS(
        registry->AddFunction(std::make_shared<TNumRows>(NAggregation::TAggregateFunction::GetFunctionName(NAggregation::EAggregate::NumRows)))
            .ok());
}

static void RegisterHouseAggregates(cp::FunctionRegistry* registry) {
#ifndef WIN32
    try {
        Y_ABORT_UNLESS(registry
                           ->AddFunction(std::make_shared<CH::WrappedAny>(
                               NAggregation::TAggregateFunction::GetHouseFunctionName(NAggregation::EAggregate::Some)))
                           .ok());
        Y_ABORT_UNLESS(registry
                           ->AddFunction(std::make_shared<CH::WrappedCount>(
                               NAggregation::TAggregateFunction::GetHouseFunctionName(NAggregation::EAggregate::Count)))
                           .ok());
        Y_ABORT_UNLESS(registry
                           ->AddFunction(std::make_shared<CH::WrappedMin>(
                               NAggregation::TAggregateFunction::GetHouseFunctionName(NAggregation::EAggregate::Min)))
                           .ok());
        Y_ABORT_UNLESS(registry
                           ->AddFunction(std::make_shared<CH::WrappedMax>(
                               NAggregation::TAggregateFunction::GetHouseFunctionName(NAggregation::EAggregate::Max)))
                           .ok());
        Y_ABORT_UNLESS(registry
                           ->AddFunction(std::make_shared<CH::WrappedSum>(
                               NAggregation::TAggregateFunction::GetHouseFunctionName(NAggregation::EAggregate::Sum)))
                           .ok());
        //Y_ABORT_UNLESS(registry->AddFunction(std::make_shared<CH::WrappedAvg>(NAggregation::TAggregateFunction::GetHouseFunctionName(NAggregation::EAggregate::Avg))).ok());
        Y_ABORT_UNLESS(registry
                           ->AddFunction(std::make_shared<CH::WrappedNumRows>(
                               NAggregation::TAggregateFunction::GetHouseFunctionName(NAggregation::EAggregate::NumRows)))
                           .ok());

        Y_ABORT_UNLESS(
            registry->AddFunction(std::make_shared<CH::ArrowGroupBy>(NAggregation::TWithKeysAggregationProcessor::GetHouseGroupByName())).ok());
    } catch (const std::exception& /*ex*/) {
        Y_ABORT_UNLESS(false);
    }
#else
    Y_UNUSED(registry);
#endif
}

static std::unique_ptr<cp::FunctionRegistry> CreateCustomRegistry() {
    auto registry = cp::FunctionRegistry::Make();
    RegisterMath(registry.get());
    RegisterRound(registry.get());
    RegisterArithmetic(registry.get());
    RegisterYdbCast(registry.get());
    RegisterCustomAggregates(registry.get());
    RegisterHouseAggregates(registry.get());
    return registry;
}

// Creates singleton custom registry
cp::FunctionRegistry* GetCustomFunctionRegistry() {
    static auto g_registry = CreateCustomRegistry();
    return g_registry.get();
}

// We want to have ExecContext per thread. All these context use one custom registry.
cp::ExecContext* GetCustomExecContext() {
    static thread_local cp::ExecContext context(arrow::default_memory_pool(), nullptr, GetCustomFunctionRegistry());
    return &context;
}

}   // namespace NKikimr::NArrow::NSSA
