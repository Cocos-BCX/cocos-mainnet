#pragma once
#include <fc/reflect/reflect.hpp>
#include <fc/variant_object.hpp>

namespace fc
{
   template<typename T>
   void to_variant( const T& o, variant& v );
   template<typename T>
   void from_variant( const variant& v, T& o );


   template<typename T>
   class to_variant_visitor
   {
      public:
         to_variant_visitor( mutable_variant_object& mvo, const T& v )
         :vo(mvo),value(v){}

         template<typename Member, class Class, Member (Class::*member)>
         void operator()( const char* name )const
         {
            this->add(vo,name,(value.*member));
         }

      private:
         template<typename M>
         void add( mutable_variant_object& vo, const char* name, const optional<M>& v )const
         { 
            if( v.valid() )
               vo(name,*v);
         }
         template<typename M>
         void add( mutable_variant_object& vo, const char* name, const M& v )const
         { vo(name,v); }

         mutable_variant_object& vo;
         const T& value;
   };

   template<typename T>
   class from_variant_visitor
   {
      public:
         from_variant_visitor( const variant_object& _vo, T& v )
         :vo(_vo),val(v){}

         template<typename Member, class Class, Member (Class::*member)>
         void operator()( const char* name )const
         {
            auto itr = vo.find(name);
            if( itr != vo.end() )
               from_variant( itr->value(), val.*member );   //nico 非枚举类型转变
         }

         const variant_object& vo;
         T& val;
   };

   template<typename IsReflected=fc::false_type>  //nico 变体转变判定是否是枚举类型 ：false_type 不是
   struct if_enum 
   {
     template<typename T>
     static inline void to_variant( const T& v, fc::variant& vo ) 
     { 
         mutable_variant_object mvo;
         fc::reflector<T>::visit( to_variant_visitor<T>( mvo, v ) );
         vo = fc::move(mvo);
     }
     template<typename T>
     static inline void from_variant( const fc::variant& v, T& o ) 
     { 
         const variant_object& vo = v.get_object();
         fc::reflector<T>::visit( from_variant_visitor<T>( vo, o ) );  //nico 从变体转变
     }
   };

    template<> 
    struct if_enum<fc::true_type> 
    {
       template<typename T>
       static inline void to_variant( const T& o, fc::variant& v ) 
       { 
           v = fc::reflector<T>::to_fc_string(o);
       }
       template<typename T>
       static inline void from_variant( const fc::variant& v, T& o ) 
       { 
           if( v.is_string() )
              o = fc::reflector<T>::from_string( v.get_string().c_str() );
           else
              o = fc::reflector<T>::from_int( v.as_int64() );
       }
    };


   template<typename T>
   void to_variant( const T& o, variant& v )
   {
      if_enum<typename fc::reflector<T>::is_enum>::to_variant( o, v );
   }

   template<typename T>
   void from_variant( const variant& v, T& o )
   {
      if_enum<typename fc::reflector<T>::is_enum>::from_variant( v, o );
   }

}
