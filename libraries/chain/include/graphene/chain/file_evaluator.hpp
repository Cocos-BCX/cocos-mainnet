/*
   created by zhangfan
   the evaluator about file
 */
#pragma once

#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace chain {

// create file
class create_file_evaluator : public evaluator<create_file_evaluator>
{
    public:
      typedef create_file_operation operation_type;

      void_result do_evaluate(const create_file_operation &o);
      object_id_result do_apply(const create_file_operation &o);
};

//
class add_file_relate_account_evaluator : public evaluator<add_file_relate_account_evaluator>
{
    public:
      typedef add_file_relate_account_operation operation_type;

      void_result do_evaluate(const add_file_relate_account_operation &o);
      void_result do_apply(const add_file_relate_account_operation &o);
};

//
class file_signature_evaluator : public evaluator<file_signature_evaluator>
{
    public:
      typedef file_signature_operation operation_type;

      void_result do_evaluate(const file_signature_operation &o);
      void_result do_apply(const file_signature_operation &o);
};

// propose relate parent file
class relate_parent_file_evaluator : public evaluator<relate_parent_file_evaluator>
{
    public:
      typedef relate_parent_file_operation operation_type;

      void_result do_evaluate(const relate_parent_file_operation &o);
      void_result do_apply(const relate_parent_file_operation &o);
      const file_object *_parent_file = nullptr;
      const file_object *_sub_file = nullptr;
};

} // namespace chain
} // namespace graphene
