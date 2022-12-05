#include "shared-ptr.h"

details::control_block::control_block(size_t strong_ref, size_t weak_ref)
    : strong_ref_(strong_ref), weak_ref_(weak_ref) {}

void details::control_block::inc_strong_ref() {
  strong_ref_++;
}

void details::control_block::inc_weak_ref() {
  weak_ref_++;
}

void details::control_block::dec_strong_ref() {
  strong_ref_--;

  if (strong_ref_ == 0) {
    unlink();
  }

  if (strong_ref_ == 0 && weak_ref_ == 0) {
    delete this;
  }
}

void details::control_block::dec_weak_ref() {
  weak_ref_--;

  if (strong_ref_ == 0 && weak_ref_ == 0) {
    delete this;
  }
}

size_t details::control_block::get_strong_ref_count() {
  return this->strong_ref_;
}
