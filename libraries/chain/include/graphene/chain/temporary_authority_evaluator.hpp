#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/protocol/temporary.hpp>
namespace graphene { namespace chain {

class temporary_authority_change_evaluator : public evaluator<temporary_authority_change_evaluator>
{
public:
   typedef temporary_authority_change_operation operation_type;
   void_result do_evaluate( const operation_type& o );
   void_result do_apply( const operation_type& o);
};


} } // graphene::chain