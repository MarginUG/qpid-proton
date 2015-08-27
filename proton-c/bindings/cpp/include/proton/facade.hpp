#ifndef PROTON_CPP_FACADE_H
#define PROTON_CPP_FACADE_H

/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

/*! \page c-and-cpp C and memory management.
 *\brief
 *
 * The C++ API is a very thin wrapper over the C API.  The C API consists of a
 * set of `struct` types and associated C functions.  For each there is a a C++
 * `facade` that provides C++ member functions to call the corresponding C
 * functions on the underlying C struct. Facade classes derive from the
 * `proton::facade` template.
 *
 * The facade class occupies no additional memory. The C+ facade pointer points
 * directly to the underlying C++ struct, calling C++ member functions corresponds
 * directly to calling the C function.
 *
 * If you want to mix C and C++ code (which should be done carefully!) you can
 * cast between a facade pointer and a C struct pointer with `proton::pn_cast`
 * and `foo::cast()` where `foo` is some C++ facade class.
 *
 * Deleting a facade object calls the appropriate `pn_foo_free` function or
 * `pn_decref` as appropriate.
 *
 * Some proton structs are reference counted, the facade classes for these
 * derive from the `proton::counted_facade` template. Most proton functions that
 * return facade objects return a reference to an object that is owned by the
 * called object. Such references are only valid in a limited scope (for example
 * in an event handler function.) To keep a reference outside that scope, you
 * can assign it to a proton::counted_ptr, std::shared_ptr, boost::shared_ptr,
 * boost::intrusive_ptr or std::unique_ptr. You can also call
 * `proton::counted_facade::new_reference` to get a raw pointer which you must
 * delete when you are done (e.g. using `std::auto_ptr`)
 */

/**@file
 * Classes and templates used by object facades.
 */

#include "proton/export.hpp"
#include "proton/config.hpp"
#include "proton/comparable.hpp"
#include <memory>
#if PN_USE_BOOST
#include "boost/shared_ptr.hpp"
#include "boost/intrusive_ptr.hpp"
#endif

namespace proton {

/**
 * Base class for C++ facades of proton C struct types.
 *
 * @see \ref c-and-cpp
 */
template <class P, class T> class facade {
  public:
    /// The underlying C struct type.
    typedef P pn_type;

    /// Cast the C struct pointer to a C++ facade pointer.
    static T* cast(P* p) { return reinterpret_cast<T*>(p); }

#if PN_USE_CPP11
    facade() = delete;
    facade(const facade&) = delete;
    facade& operator=(const facade&) = delete;
    void operator delete(void* p) = delete; // Defined by type T.
#else
  private:
    facade();
    facade(const facade&);
    facade& operator=(const facade&);
    void operator delete(void* p);
#endif
};

///@cond INTERNAL Cast from a C++ facade type to the corresponding proton C struct type.
/// Allow casting away const, the underlying pn structs have not constness.
template <class T> typename T::pn_type* pn_cast(const T* p) {
    return reinterpret_cast<typename T::pn_type*>(const_cast<T*>(p));
}
///@endcond

/**
 * Smart pointer for object derived from `proton::counted_facade`.
 *
 * You can use it in your own code if you or convert it to any of
 * std::shared_ptr, boost::shared_ptr, boost::intrusive_ptr or std::unique_ptr
 *
 * Note a std::unique_ptr only takes ownership of a a
 * *reference*, not the underlying struct itself.
*/
template <class T> class counted_ptr : public comparable<counted_ptr<T> > {
  public:
    typedef T element_type;

    explicit counted_ptr(T *p = 0, bool add_ref = true) : ptr_(p) { if (p && add_ref) incref(ptr_); }

    counted_ptr(const counted_ptr<T>& p) : ptr_(p.ptr_) { incref(ptr_); }

    // TODO aconway 2015-08-20: C++11 move constructor

    ~counted_ptr() { decref(ptr_); }

    void swap(counted_ptr& x) { std::swap(ptr_, x.ptr_); }

    counted_ptr<T>& operator=(const counted_ptr<T>& p) {
        counted_ptr<T>(p.get()).swap(*this);
        return *this;
    }

    void reset(T* p=0, bool add_ref = true) {
        counted_ptr<T>(p, add_ref).swap(*this);
    }

    T* release() {
        T* ret = ptr_;
        ptr_ = 0;
        return ret;
    }

    T* get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    operator bool() const { return ptr_; }
    bool operator!() const { return !ptr_; }

    template <class U> operator counted_ptr<U>() const { return counted_ptr<U>(get()); }
    template <class U> bool operator==(const counted_ptr<U>& x) { return get() == x.get(); }
    template <class U> bool operator<(const counted_ptr<U>& x) { return get() < x.get(); }

  private:
    T* ptr_;
};

#if PN_USE_BOOST
template <class T> inline void intrusive_ptr_add_ref(const T* p) { incref(p); }
template <class T> inline void intrusive_ptr_release(const T* p) { decref(p); }
#endif

///@cond INTERNAL
class pn_counted {};
void incref(const pn_counted*);
void decref(const pn_counted*);
///@endcond

/// Reference counted proton types are convertible to smart pointer types.
template <class T> class ptr_convetible {
 public:
    operator counted_ptr<T>() { return counted_ptr<T>(static_cast<T*>(this)); }
    operator counted_ptr<const T>() const { return counted_ptr<const T>(static_cast<const T*>(this)); }
#if PN_USE_CPP11
    // TODO aconway 2015-08-21: need weak pointer context for efficient shared_ptr
    operator std::shared_ptr<T>() { return std::shared_ptr<T>(incref(this)); }
    operator std::shared_ptr<const T>() const { return std::shared_ptr<const T>(incref(this)); }
    operator std::unique_ptr<T>() { return std::unique_ptr<T>(incref(this)); }
    operator std::unique_ptr<const T>() const { return std::unique_ptr<const T>(incref(this)); }
#endif
#if PN_USE_BOOST
    // TODO aconway 2015-08-21: need weak pointer context for efficient shared_ptr
    operator boost::shared_ptr<T>() { return boost::shared_ptr<T>(incref(this)); }
    operator boost::shared_ptr<const T>() const { return boost::shared_ptr<const T>(incref(this)); }
    operator boost::intrusive_ptr<T>() { return boost::intrusive_ptr<T>(this); }
    operator boost::intrusive_ptr<const T>() const { return boost::intrusive_ptr<const T>(this); }
#endif

  private:
    T* incref() { proton::incref(static_cast<T*>(this)); return static_cast<T*>(this); }
    const T* incref() const { proton::incref(this); return static_cast<const T*>(this); }
};

/**
 * Some proton C structs are reference counted. The C++ facade for such structs can be
 * converted to any of the following smart pointers: std::shared_ptr, std::unique_ptr,
 * boost::shared_ptr, boost::intrusive_ptr.
 *
 * unique_ptr takes ownership of a single *reference* not the underlying struct,
 * so it is safe to have multiple unique_ptr to the same facade object or to mix
 * unique_ptr with shared_ptr etc.
 *
 * Deleting a counted_facade subclass actually calls `pn_decref` to remove a reference.
 */
template <class P, class T> class counted_facade :
        public facade<P, T>, public pn_counted, public ptr_convetible<T>
{
  public:

    /// Deleting a counted_facade actually calls `pn_decref` to remove a reference.
    void operator delete(void* p) { decref(reinterpret_cast<pn_counted*>(p)); }

    operator counted_ptr<T>() { return counted_ptr<T>(static_cast<T*>(this)); }
    operator counted_ptr<const T>() const { return counted_ptr<const T>(static_cast<const T*>(this)); }
#if PN_USE_CPP11
    // TODO aconway 2015-08-21: need weak pointer context for efficient shared_ptr
    operator std::shared_ptr<T>() { return std::shared_ptr<T>(new_reference(this)); }
    operator std::shared_ptr<const T>() const { return std::shared_ptr<const T>(new_reference(this)); }
    operator std::unique_ptr<T>() { return std::unique_ptr<T>(new_reference(this)); }
    operator std::unique_ptr<const T>() const { return std::unique_ptr<const T>(new_reference(this)); }
#endif
#if PN_USE_BOOST
    // TODO aconway 2015-08-21: need weak pointer context for efficient shared_ptr
    operator boost::shared_ptr<T>() { return boost::shared_ptr<T>(new_reference(this)); }
    operator boost::shared_ptr<const T>() const { return boost::shared_ptr<const T>(new_reference(this)); }
    operator boost::intrusive_ptr<T>() { return boost::intrusive_ptr<T>(this); }
    operator boost::intrusive_ptr<const T>() const { return boost::intrusive_ptr<const T>(this); }
#endif

    /** Get a pointer to a new reference to the underlying object.
     * You must delete the returned pointer to release the reference.
     * It is safer to convert to one of the supported smart pointer types.
     */
    T* new_reference() { proton::incref(this); return static_cast<T*>(this); }
    const T* new_reference() const { proton::incref(this); return static_cast<const T*>(this); }

  private:
    counted_facade(const counted_facade&);
    counted_facade& operator=(const counted_facade&);
};

///@cond INTERNAL
///Base class for reference counted objects other than proton struct facade types.
class counted {
  protected:
    counted();
    virtual ~counted();

  private:
    counted(const counted&);
    counted& operator=(const counted&);
    int refcount_;

    friend void incref(const counted* p);
    friend void decref(const counted* p);
  template <class T> friend class counted_ptr;
};

void incref(const counted* p);
void decref(const counted* p);
///@endcond

}
#endif  /*!PROTON_CPP_FACADE_H*/
