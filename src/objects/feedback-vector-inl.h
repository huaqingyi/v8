// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FEEDBACK_VECTOR_INL_H_
#define V8_OBJECTS_FEEDBACK_VECTOR_INL_H_

#include "src/common/globals.h"
#include "src/heap/factory-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/code-inl.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/maybe-object-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/smi.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TQ_OBJECT_CONSTRUCTORS_IMPL(FeedbackVector)
OBJECT_CONSTRUCTORS_IMPL(FeedbackMetadata, HeapObject)
OBJECT_CONSTRUCTORS_IMPL(ClosureFeedbackCellArray, FixedArray)

NEVER_READ_ONLY_SPACE_IMPL(FeedbackVector)
NEVER_READ_ONLY_SPACE_IMPL(ClosureFeedbackCellArray)

CAST_ACCESSOR(FeedbackMetadata)
CAST_ACCESSOR(ClosureFeedbackCellArray)

INT32_ACCESSORS(FeedbackMetadata, slot_count, kSlotCountOffset)

INT32_ACCESSORS(FeedbackMetadata, closure_feedback_cell_count,
                kFeedbackCellCountOffset)

int32_t FeedbackMetadata::synchronized_slot_count() const {
  return base::Acquire_Load(reinterpret_cast<const base::Atomic32*>(
      FIELD_ADDR(*this, kSlotCountOffset)));
}

int32_t FeedbackMetadata::get(int index) const {
  DCHECK(index >= 0 && index < length());
  int offset = kHeaderSize + index * kInt32Size;
  return ReadField<int32_t>(offset);
}

void FeedbackMetadata::set(int index, int32_t value) {
  DCHECK(index >= 0 && index < length());
  int offset = kHeaderSize + index * kInt32Size;
  WriteField<int32_t>(offset, value);
}

bool FeedbackMetadata::is_empty() const { return slot_count() == 0; }

int FeedbackMetadata::length() const {
  return FeedbackMetadata::length(slot_count());
}

int FeedbackMetadata::GetSlotSize(FeedbackSlotKind kind) {
  switch (kind) {
    case FeedbackSlotKind::kForIn:
    case FeedbackSlotKind::kInstanceOf:
    case FeedbackSlotKind::kCompareOp:
    case FeedbackSlotKind::kBinaryOp:
    case FeedbackSlotKind::kLiteral:
    case FeedbackSlotKind::kTypeProfile:
      return 1;

    case FeedbackSlotKind::kCall:
    case FeedbackSlotKind::kCloneObject:
    case FeedbackSlotKind::kLoadProperty:
    case FeedbackSlotKind::kLoadGlobalInsideTypeof:
    case FeedbackSlotKind::kLoadGlobalNotInsideTypeof:
    case FeedbackSlotKind::kLoadKeyed:
    case FeedbackSlotKind::kHasKeyed:
    case FeedbackSlotKind::kStoreNamedSloppy:
    case FeedbackSlotKind::kStoreNamedStrict:
    case FeedbackSlotKind::kStoreOwnNamed:
    case FeedbackSlotKind::kStoreGlobalSloppy:
    case FeedbackSlotKind::kStoreGlobalStrict:
    case FeedbackSlotKind::kStoreKeyedSloppy:
    case FeedbackSlotKind::kStoreKeyedStrict:
    case FeedbackSlotKind::kStoreInArrayLiteral:
    case FeedbackSlotKind::kStoreDataPropertyInLiteral:
      return 2;

    case FeedbackSlotKind::kInvalid:
    case FeedbackSlotKind::kKindsNumber:
      UNREACHABLE();
  }
  return 1;
}

Handle<FeedbackCell> ClosureFeedbackCellArray::GetFeedbackCell(int index) {
  return handle(FeedbackCell::cast(get(index)), GetIsolate());
}

void FeedbackVector::clear_padding() { set_padding(0); }

bool FeedbackVector::is_empty() const { return length() == 0; }

FeedbackMetadata FeedbackVector::metadata() const {
  return shared_function_info().feedback_metadata();
}

void FeedbackVector::clear_invocation_count() { set_invocation_count(0); }

Code FeedbackVector::optimized_code() const {
  MaybeObject slot = optimized_code_weak_or_smi();
  DCHECK(slot->IsSmi() || slot->IsWeakOrCleared());
  HeapObject heap_object;
  return slot->GetHeapObject(&heap_object) ? Code::cast(heap_object) : Code();
}

OptimizationMarker FeedbackVector::optimization_marker() const {
  MaybeObject slot = optimized_code_weak_or_smi();
  Smi value;
  if (!slot->ToSmi(&value)) return OptimizationMarker::kNone;
  return static_cast<OptimizationMarker>(value.value());
}

bool FeedbackVector::has_optimized_code() const {
  return !optimized_code().is_null();
}

bool FeedbackVector::has_optimization_marker() const {
  return optimization_marker() != OptimizationMarker::kLogFirstExecution &&
         optimization_marker() != OptimizationMarker::kNone;
}

// Conversion from an integer index to either a slot or an ic slot.
// static
FeedbackSlot FeedbackVector::ToSlot(intptr_t index) {
  DCHECK_LE(static_cast<uintptr_t>(index),
            static_cast<uintptr_t>(std::numeric_limits<int>::max()));
  return FeedbackSlot(static_cast<int>(index));
}

MaybeObject FeedbackVector::Get(FeedbackSlot slot) const {
  return raw_feedback_slots(GetIndex(slot));
}

MaybeObject FeedbackVector::Get(IsolateRoot isolate, FeedbackSlot slot) const {
  return raw_feedback_slots(isolate, GetIndex(slot));
}

Handle<FeedbackCell> FeedbackVector::GetClosureFeedbackCell(int index) const {
  DCHECK_GE(index, 0);
  ClosureFeedbackCellArray cell_array =
      ClosureFeedbackCellArray::cast(closure_feedback_cell_array());
  return cell_array.GetFeedbackCell(index);
}

void FeedbackVector::Set(FeedbackSlot slot, MaybeObject value,
                         WriteBarrierMode mode) {
  set_raw_feedback_slots(GetIndex(slot), value, mode);
}

void FeedbackVector::Set(FeedbackSlot slot, Object value,
                         WriteBarrierMode mode) {
  set_raw_feedback_slots(GetIndex(slot), MaybeObject::FromObject(value), mode);
}

inline MaybeObjectSlot FeedbackVector::slots_start() {
  return RawMaybeWeakField(OffsetOfElementAt(0));
}

// Helper function to transform the feedback to BinaryOperationHint.
BinaryOperationHint BinaryOperationHintFromFeedback(int type_feedback) {
  switch (type_feedback) {
    case BinaryOperationFeedback::kNone:
      return BinaryOperationHint::kNone;
    case BinaryOperationFeedback::kSignedSmall:
      return BinaryOperationHint::kSignedSmall;
    case BinaryOperationFeedback::kSignedSmallInputs:
      return BinaryOperationHint::kSignedSmallInputs;
    case BinaryOperationFeedback::kNumber:
      return BinaryOperationHint::kNumber;
    case BinaryOperationFeedback::kNumberOrOddball:
      return BinaryOperationHint::kNumberOrOddball;
    case BinaryOperationFeedback::kString:
      return BinaryOperationHint::kString;
    case BinaryOperationFeedback::kBigInt:
      return BinaryOperationHint::kBigInt;
    default:
      return BinaryOperationHint::kAny;
  }
  UNREACHABLE();
}

// Helper function to transform the feedback to CompareOperationHint.
template <CompareOperationFeedback::Type Feedback>
bool Is(int type_feedback) {
  return !(type_feedback & ~Feedback);
}

CompareOperationHint CompareOperationHintFromFeedback(int type_feedback) {
  if (Is<CompareOperationFeedback::kNone>(type_feedback)) {
    return CompareOperationHint::kNone;
  }

  if (Is<CompareOperationFeedback::kSignedSmall>(type_feedback)) {
    return CompareOperationHint::kSignedSmall;
  } else if (Is<CompareOperationFeedback::kNumber>(type_feedback)) {
    return CompareOperationHint::kNumber;
  } else if (Is<CompareOperationFeedback::kNumberOrBoolean>(type_feedback)) {
    return CompareOperationHint::kNumberOrBoolean;
  }

  if (Is<CompareOperationFeedback::kInternalizedString>(type_feedback)) {
    return CompareOperationHint::kInternalizedString;
  } else if (Is<CompareOperationFeedback::kString>(type_feedback)) {
    return CompareOperationHint::kString;
  }

  if (Is<CompareOperationFeedback::kReceiver>(type_feedback)) {
    return CompareOperationHint::kReceiver;
  } else if (Is<CompareOperationFeedback::kReceiverOrNullOrUndefined>(
                 type_feedback)) {
    return CompareOperationHint::kReceiverOrNullOrUndefined;
  }

  if (Is<CompareOperationFeedback::kBigInt>(type_feedback)) {
    return CompareOperationHint::kBigInt;
  }

  if (Is<CompareOperationFeedback::kSymbol>(type_feedback)) {
    return CompareOperationHint::kSymbol;
  }

  DCHECK(Is<CompareOperationFeedback::kAny>(type_feedback));
  return CompareOperationHint::kAny;
}

// Helper function to transform the feedback to ForInHint.
ForInHint ForInHintFromFeedback(ForInFeedback type_feedback) {
  switch (type_feedback) {
    case ForInFeedback::kNone:
      return ForInHint::kNone;
    case ForInFeedback::kEnumCacheKeys:
      return ForInHint::kEnumCacheKeys;
    case ForInFeedback::kEnumCacheKeysAndIndices:
      return ForInHint::kEnumCacheKeysAndIndices;
    default:
      return ForInHint::kAny;
  }
  UNREACHABLE();
}

Handle<Symbol> FeedbackVector::UninitializedSentinel(Isolate* isolate) {
  return isolate->factory()->uninitialized_symbol();
}

Handle<Symbol> FeedbackVector::GenericSentinel(Isolate* isolate) {
  return isolate->factory()->generic_symbol();
}

Handle<Symbol> FeedbackVector::MegamorphicSentinel(Isolate* isolate) {
  return isolate->factory()->megamorphic_symbol();
}

Symbol FeedbackVector::RawUninitializedSentinel(Isolate* isolate) {
  return ReadOnlyRoots(isolate).uninitialized_symbol();
}

bool FeedbackMetadataIterator::HasNext() const {
  return next_slot_.ToInt() < metadata().slot_count();
}

FeedbackSlot FeedbackMetadataIterator::Next() {
  DCHECK(HasNext());
  cur_slot_ = next_slot_;
  slot_kind_ = metadata().GetKind(cur_slot_);
  next_slot_ = FeedbackSlot(next_slot_.ToInt() + entry_size());
  return cur_slot_;
}

int FeedbackMetadataIterator::entry_size() const {
  return FeedbackMetadata::GetSlotSize(kind());
}

MaybeObject FeedbackNexus::GetFeedback() const {
  MaybeObject feedback = vector().Get(slot());
  FeedbackVector::AssertNoLegacyTypes(feedback);
  return feedback;
}

MaybeObject FeedbackNexus::GetFeedbackExtra() const {
#ifdef DEBUG
  FeedbackSlotKind kind = vector().GetKind(slot());
  DCHECK_LT(1, FeedbackMetadata::GetSlotSize(kind));
#endif
  return vector().Get(slot().WithOffset(1));
}

void FeedbackNexus::SetFeedback(Object feedback, WriteBarrierMode mode) {
  SetFeedback(MaybeObject::FromObject(feedback));
}

void FeedbackNexus::SetFeedback(MaybeObject feedback, WriteBarrierMode mode) {
  FeedbackVector::AssertNoLegacyTypes(feedback);
  vector().Set(slot(), feedback, mode);
}

void FeedbackNexus::SetFeedbackExtra(Object feedback_extra,
                                     WriteBarrierMode mode) {
#ifdef DEBUG
  FeedbackSlotKind kind = vector().GetKind(slot());
  DCHECK_LT(1, FeedbackMetadata::GetSlotSize(kind));
  FeedbackVector::AssertNoLegacyTypes(MaybeObject::FromObject(feedback_extra));
#endif
  vector().Set(slot().WithOffset(1), MaybeObject::FromObject(feedback_extra),
               mode);
}

void FeedbackNexus::SetFeedbackExtra(MaybeObject feedback_extra,
                                     WriteBarrierMode mode) {
#ifdef DEBUG
  FeedbackVector::AssertNoLegacyTypes(feedback_extra);
#endif
  vector().Set(slot().WithOffset(1), feedback_extra, mode);
}

Isolate* FeedbackNexus::GetIsolate() const { return vector().GetIsolate(); }
}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FEEDBACK_VECTOR_INL_H_
