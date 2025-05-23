#include "fluent_log.h"

namespace NYT::NLogging {

using namespace NYson;
using namespace NYTree;
using namespace NLogging;

////////////////////////////////////////////////////////////////////////////////

TOneShotFluentLogEvent::TOneShotFluentLogEvent(
    TStatePtr state,
    const NLogging::TLogger& logger,
    NLogging::ELogLevel level)
    : TBase(state->GetConsumer())
    , State_(std::move(state))
    , Logger_(&logger)
    , Level_(level)
{
    for (const auto& [key, value] : logger.GetStructuredTags()) {
        (*this).Item(key).Value(value);
    }
}

TOneShotFluentLogEvent::~TOneShotFluentLogEvent()
{
    if (State_ && *Logger_) {
        LogStructuredEvent(*Logger_, State_->GetValue(), Level_);
    }
}

////////////////////////////////////////////////////////////////////////////////

TOneShotFluentLogEvent LogStructuredEventFluently(const TLogger& logger, ELogLevel level)
{
    return TOneShotFluentLogEvent(
        New<TFluentYsonWriterState>(EYsonFormat::Binary, EYsonType::MapFragment),
        logger,
        level);
}

TOneShotFluentLogEvent LogStructuredEventFluentlyToNowhere()
{
    static const TLogger NullLogger;
    return TOneShotFluentLogEvent(
        New<TFluentYsonWriterState>(EYsonFormat::Binary, EYsonType::MapFragment),
        NullLogger,
        ELogLevel::Debug);
}

////////////////////////////////////////////////////////////////////////////////

TStructuredLogBatcher::TStructuredLogBatcher(TLogger logger, i64 maxBatchSize, ELogLevel level)
    : Logger(std::move(logger))
    , MaxBatchSize_(maxBatchSize)
    , Level_(level)
{ }

TStructuredLogBatcher::TFluent TStructuredLogBatcher::AddItemFluently()
{
    if (std::ssize(BatchYson_) >= MaxBatchSize_) {
        Flush();
    }
    ++BatchItemCount_;

    return BuildYsonListFragmentFluently(&BatchYsonWriter_)
        .Item();
}

void TStructuredLogBatcher::Flush()
{
    if (BatchItemCount_ == 0) {
        return;
    }
    BatchYsonWriter_.Flush();
    LogStructuredEventFluently(Logger, Level_)
        .Item("batch")
            .BeginList()
                .Do([&] (TFluentList fluent) {
                    fluent.GetConsumer()->OnRaw(TYsonString(std::move(BatchYson_), EYsonType::ListFragment));
                })
            .EndList();
    BatchYson_.clear();
    BatchItemCount_ = 0;
}

TStructuredLogBatcher::~TStructuredLogBatcher()
{
    Flush();
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NLogging
