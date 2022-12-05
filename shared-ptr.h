#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace details {

class control_block {
public:
  control_block(size_t strong_ref, size_t weak_ref);
  virtual ~control_block() = default;
  virtual void unlink() = 0;

  void inc_strong_ref();
  void inc_weak_ref();
  void dec_strong_ref();
  void dec_weak_ref();

  size_t get_strong_ref_count();

private:
  size_t strong_ref_;
  size_t weak_ref_;
};

template <typename T, typename D>
struct ptr_block : public control_block {
  ptr_block() : control_block(0, 0), ptr(nullptr) {}

  explicit ptr_block(T* ptr, D deleter)
      : control_block(1, 0), ptr(ptr), deleter(std::move(deleter)) {}

  ~ptr_block() = default;

  void unlink() {
    deleter(ptr);
  }

private:
  T* ptr;
  D deleter;
};

template <typename T>
struct obj_block : public control_block {
public:
  template <typename... Args>
  explicit obj_block(Args&&... args) : control_block(1, 0) {
    new (&data) T(std::forward<Args>(args)...);
  }

  T* get_ptr() {
    return reinterpret_cast<T*>(&data);
  }

  void unlink() {
    reinterpret_cast<T*>(&data)->~T();
  }

private:
  std::aligned_storage_t<sizeof(T), alignof(T)> data;
};
} // namespace details

template <typename T>
class weak_ptr;

template <typename T>
class shared_ptr {
  template <typename Y>
  friend class shared_ptr;

  template <typename Y>
  friend class weak_ptr;

public:
  explicit shared_ptr(details::obj_block<T>* cb)
      : cb(static_cast<details::control_block*>(cb)), ptr(cb->get_ptr()) {}

  shared_ptr() noexcept = default;
  shared_ptr(std::nullptr_t) noexcept {}

  template <typename Y, typename D = std::default_delete<Y>,
            typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
  shared_ptr(Y* ptr, D&& deleter = D()) try
      : cb(new details::ptr_block<Y, D>(ptr, std::forward<D>(deleter))),
        ptr(static_cast<T*>(ptr)) {
  } catch (...) {
    deleter(ptr);
    throw;
  }

  template <typename Y>
  shared_ptr(const shared_ptr<Y>& other, T* ptr) noexcept
      : cb(other.cb), ptr(ptr) {
    if (cb) {
      cb->inc_strong_ref();
    }
  }

  template <typename Y,
            typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
  shared_ptr(const shared_ptr<Y>& other) noexcept
      : cb(other.cb), ptr(static_cast<T*>(other.ptr)) {
    if (cb) {
      cb->inc_strong_ref();
    }
  }

  template <typename Y,
            typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
  shared_ptr& operator=(const shared_ptr<Y>& other) noexcept {
    if (this != &other) {
      shared_ptr(other).swap(*this);
    }
    return *this;
  }

  template <typename Y,
            typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
  shared_ptr(shared_ptr<Y>&& other) noexcept
      : cb(other.cb), ptr(static_cast<T*>(other.ptr)) {
    other.cb = other.ptr = nullptr;
  }

  template <typename Y,
            typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
  shared_ptr& operator=(shared_ptr<Y>&& other) noexcept {
    if (this != &other) {
      shared_ptr(std::move(other)).swap(*this);
    }
    return *this;
  }

  ~shared_ptr() {
    if (cb) {
      cb->dec_strong_ref();
    }
  }

  shared_ptr(const shared_ptr& other) noexcept : cb(other.cb), ptr(other.ptr) {
    if (cb) {
      cb->inc_strong_ref();
    }
  }

  shared_ptr& operator=(const shared_ptr& other) noexcept {
    if (this != &other) {
      shared_ptr(other).swap(*this);
    }
    return *this;
  }

  shared_ptr(shared_ptr&& other) noexcept : cb(other.cb), ptr(other.ptr) {
    other.cb = nullptr;
    other.ptr = nullptr;
  }

  shared_ptr& operator=(shared_ptr&& other) noexcept {
    if (this != &other) {
      shared_ptr(std::move(other)).swap(*this);
    }
    return *this;
  }

  T* get() const noexcept {
    return ptr;
  }

  operator bool() const noexcept {
    return get() != nullptr;
  }

  T& operator*() const noexcept {
    return *get();
  }

  T* operator->() const noexcept {
    return get();
  }

  friend bool operator==(const shared_ptr& a, const shared_ptr& b) {
    return a.ptr == b.ptr;
  }

  friend bool operator!=(const shared_ptr& a, const shared_ptr& b) {
    return a.ptr != b.ptr;
  }

  std::size_t use_count() const noexcept {
    return (cb != nullptr ? cb->get_strong_ref_count() : 0);
  }

  void reset() noexcept {
    shared_ptr().swap(*this);
  }

  template <typename Y, typename D = std::default_delete<Y>,
            typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
  void reset(Y* new_ptr, D&& new_deleter = D()) {
    shared_ptr(new_ptr, std::forward<D>(new_deleter)).swap(*this);
  }

  void swap(shared_ptr& other) {
    std::swap(cb, other.cb);
    std::swap(ptr, other.ptr);
  }

private:
  details::control_block* cb{nullptr};
  T* ptr{nullptr};
};

template <typename T>
class weak_ptr {
public:
  weak_ptr() noexcept = default;

  weak_ptr(const weak_ptr& other) noexcept : cb(other.cb), ptr(other.ptr) {
    if (cb) {
      cb->inc_weak_ref();
    }
  }

  weak_ptr& operator=(const weak_ptr& other) noexcept {
    if (this != &other) {
      weak_ptr(other).swap(*this);
    }
    return *this;
  }

  weak_ptr(weak_ptr&& other) noexcept : cb(other.cb), ptr(other.ptr) {
    other.cb = nullptr;
    other.ptr = nullptr;
  }

  weak_ptr& operator=(weak_ptr&& other) noexcept {
    if (this != &other) {
      weak_ptr(std::move(other)).swap(*this);
    }
    return *this;
  }

  weak_ptr(const shared_ptr<T>& other) noexcept : cb(other.cb), ptr(other.ptr) {
    if (cb) {
      cb->inc_weak_ref();
    }
  }

  weak_ptr& operator=(const shared_ptr<T>& other) noexcept {
    weak_ptr(other).swap(*this);
    return *this;
  }

  ~weak_ptr() {
    if (cb) {
      cb->dec_weak_ref();
    }
  }

  shared_ptr<T> lock() const noexcept {
    if (cb == nullptr || cb->get_strong_ref_count() == 0) {
      return shared_ptr<T>();
    }
    shared_ptr<T> tmp = shared_ptr<T>();
    tmp.ptr = ptr;
    tmp.cb = cb;
    cb->inc_strong_ref();
    return tmp;
  }

  void swap(weak_ptr& other) noexcept {
    std::swap(cb, other.cb);
    std::swap(ptr, other.ptr);
  }

private:
  details::control_block* cb{nullptr};
  T* ptr{nullptr};
};

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
  return shared_ptr<T>(new details::obj_block<T>(std::forward<Args>(args)...));
}
