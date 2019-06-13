/*
Copyright (c) 2013, Pierre KRIEGER
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * This file is a replacement for the missing C++11 features of MSVC++
 */
#ifndef INCLUDE_EXCEPTION_HPP
#define INCLUDE_EXCEPTION_HPP

#include <exception>
#include <type_traits>

#ifdef _MSC_VER
#   define EXCEPTION_HPP_NORETURN_MACRO __declspec(noreturn)
#else
#   define EXCEPTION_HPP_NORETURN_MACRO [[noreturn]]
#endif

namespace std {
    class nested_exception {
    public:
        nested_exception() : nested(current_exception()) {}
        virtual ~nested_exception() {}

        EXCEPTION_HPP_NORETURN_MACRO
        void rethrow_nested() const         { std::rethrow_exception(nested); }

        exception_ptr nested_ptr() const        { return nested; }


    private:
        exception_ptr nested;
    };

    template<typename T>
    EXCEPTION_HPP_NORETURN_MACRO
    void throw_with_nested(T&& t)
    {
        typedef remove_reference<T>::type RealT;

        struct ThrowWithNestedExcept : nested_exception, RealT
        {
            ThrowWithNestedExcept(T&& t) : RealT(std::forward<T>(t)) {}
        };

        if (is_base_of<nested_exception,RealT>::value)
            throw std::forward<T>(t);
        else
            throw ThrowWithNestedExcept(std::forward<T>(t));
    }

    template<typename E>
    void rethrow_if_nested(const E& e)
    {
        const auto ptr = dynamic_cast<const std::nested_exception*>(&e);
        if (ptr) ptr->rethrow_nested();
    }
}

#endif
