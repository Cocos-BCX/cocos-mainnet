/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <string>

#include <fc/container/flat.hpp>
#include <fc/reflect/reflect.hpp>

namespace graphene { namespace chain {

/**
 * @brief An ID for some votable object
 *
 * This class stores an ID for a votable object. The ID is comprised of two fields: a type, and an instance. The
 * type field stores which kind of object is being voted on, and the instance stores which specific object of that
 * type is being referenced by this ID.
 *
 * A value of vote_id_type is implicitly convertible to an unsigned 32-bit integer containing only the instance. It
 * may also be implicitly assigned from a uint32_t, which will update the instance. It may not, however, be
 * implicitly constructed from a uint32_t, as in this case, the type would be unknown.
 *
 * On the wire, a vote_id_type is represented as a 32-bit integer with the type in the lower 8 bits and the instance
 * in the upper 24 bits. This means that types may never exceed 8 bits, and instances may never exceed 24 bits.
 *
 * In JSON, a vote_id_type is represented as a string "type:instance", i.e. "1:5" would be type 1 and instance 5.
 *
 * @note In the Graphene protocol, vote_id_type instances are unique across types; that is to say, if an object of
 * type 1 has instance 4, an object of type 0 may not also have instance 4. In other words, the type is not a
 * namespace for instances; it is only an informational field.
 */
struct vote_id_type
{
   /// Lower 8 bits are type; upper 24 bits are instance
   uint32_t content;

   friend size_t hash_value( vote_id_type v ) { return std::hash<uint32_t>()(v.content); }
   enum vote_type
   {
      committee,
      witness,
      worker,
      VOTE_TYPE_COUNT
   };

   /// Default constructor. Sets type and instance to 0
   vote_id_type():content(0){}
   /// Construct this vote_id_type with provided type and instance
   vote_id_type(vote_type type, uint32_t instance = 0)
      : content(instance<<8 | type)
   {}
   /// Construct this vote_id_type from a serial string in the form "type:instance"
   explicit vote_id_type(const std::string& serial)
   {
      auto colon = serial.find(':');
      if( colon != std::string::npos )
         *this = vote_id_type(vote_type(std::stoul(serial.substr(0, colon))), std::stoul(serial.substr(colon+1)));
   }

   /// Set the type of this vote_id_type
   void set_type(vote_type type)
   {
      content &= 0xffffff00;
      content |= type & 0xff;
   }
   /// Get the type of this vote_id_type
   vote_type type()const
   {
      return vote_type(content & 0xff);
   }

   /// Set the instance of this vote_id_type
   void set_instance(uint32_t instance)
   {
      assert(instance < 0x01000000);
      content &= 0xff;
      content |= instance << 8;
   }
   /// Get the instance of this vote_id_type
   uint32_t instance()const
   {
      return content >> 8;
   }

   vote_id_type& operator =(vote_id_type other)
   {
      content = other.content;
      return *this;
   }
   /// Set the instance of this vote_id_type
   vote_id_type& operator =(uint32_t instance)
   {
      set_instance(instance);
      return *this;
   }
   /// Get the instance of this vote_id_type
   operator uint32_t()const
   {
      return instance();
   }

   /// Convert this vote_id_type to a serial string in the form "type:instance"
   explicit operator std::string()const
   {
      return std::to_string(type()) + ":" + std::to_string(instance());
   }
};

class global_property_object;

vote_id_type get_next_vote_id( global_property_object& gpo, vote_id_type::vote_type type );

} } // graphene::chain

namespace fc
{

class variant;

void to_variant( const graphene::chain::vote_id_type& var, fc::variant& vo );
void from_variant( const fc::variant& var, graphene::chain::vote_id_type& vo );

} // fc

FC_REFLECT_TYPENAME( fc::flat_set<graphene::chain::vote_id_type> )

FC_REFLECT_ENUM( graphene::chain::vote_id_type::vote_type, (witness)(committee)(worker)(VOTE_TYPE_COUNT) )
FC_REFLECT( graphene::chain::vote_id_type, (content) )
