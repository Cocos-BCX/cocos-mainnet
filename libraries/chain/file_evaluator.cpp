/*
   created by zhangfan
   the evaluator about file
 */

#include <graphene/chain/file_evaluator.hpp>
#include <graphene/chain/file_object.hpp>
#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/chain/exceptions.hpp>

#include <fc/smart_ref_impl.hpp>

#include <fc/log/logger.hpp>

namespace graphene
{
namespace chain
{

void_result create_file_evaluator::do_evaluate(const create_file_operation &o)
{
    return void_result();
}

object_id_result create_file_evaluator::do_apply(const create_file_operation &o)
{
  try
  {
    database &d = db();
    // create file object
    const file_object &file_obj = d.create<file_object>([&](file_object &file) {
      file.file_name = o.file_name;
      file.file_owner = o.file_owner;
      file.create_time = d.head_block_time();
      file.file_content = o.file_content;
    });
    return file_obj.id;
  }
  FC_CAPTURE_AND_RETHROW((o))
}

void_result add_file_relate_account_evaluator::do_evaluate(const add_file_relate_account_operation &o)
{
  database &d = db();
  const auto &file_obj = o.file_id(d);
  FC_ASSERT(file_obj.file_owner == o.file_owner, "This account is not the owner of this file.");
  FC_ASSERT(file_obj.related_account.empty(), "This file had be added related account.");
  return void_result();
}

void_result add_file_relate_account_evaluator::do_apply(const add_file_relate_account_operation &o)
{
  try
  {
    database &d = db();
    // add the file's related account
    d.modify(o.file_id(d), [&](file_object &file_obj) {
      file_obj.related_account = o.related_account;
    });
    return void_result();
  }
  FC_CAPTURE_AND_RETHROW((o))
}

void_result file_signature_evaluator::do_evaluate(const file_signature_operation &o)
{
  database &d = db();
  const auto &file_obj = o.file_id(d);
  FC_ASSERT(file_obj.related_account.find(o.signature_account) != file_obj.related_account.end(), "This account is not the related account of this file.");
  FC_ASSERT(file_obj.signature.find(o.signature_account) == file_obj.signature.end(), "This account had signatured this file.");
  return void_result();
}

void_result file_signature_evaluator::do_apply(const file_signature_operation &o)
{
  try
  {
    database &d = db();
    // add the signature info into the file
    d.modify(o.file_id(d), [&](file_object &file_obj) {
      file_obj.signature.insert(std::make_pair(o.signature_account, o.signature));
    });
    return void_result();
  }
  FC_CAPTURE_AND_RETHROW((o))
}

void_result relate_parent_file_evaluator::do_evaluate(const relate_parent_file_operation &o)
{
  database &d = db();
  const file_object &parent_file_obj = o.parent_file(d);

  // check out the creator of the parent file
  FC_ASSERT(parent_file_obj.file_owner == o.parent_file_owner, "The parent file's owner is error");

  const file_object &sub_file_obj = o.sub_file(d);

  // Verify that the sub file is already related to the parent file
  FC_ASSERT(!(sub_file_obj.parent_file), "The sub file had related to a parent file.");

  // Verify that if the proposer is the sub file's creator
  FC_ASSERT(sub_file_obj.file_owner == o.sub_file_owner, "You are not the sub file's owner.");

  return void_result();
}


void_result relate_parent_file_evaluator::do_apply(const relate_parent_file_operation &o)
{
  try
  {
    database &d = db();
    // add sub file into the parent file
    d.modify(o.parent_file(d), [&](file_object &parent_file_obj) {
      parent_file_obj.sub_file.push_back(o.sub_file);
    });
    // add parent file into the sub file
    d.modify(o.sub_file(d), [&](file_object &sub_file_obj) {
      sub_file_obj.parent_file = o.parent_file;
    });
    return void_result();
  }
  FC_CAPTURE_AND_RETHROW((o))
}

} // namespace chain
} // namespace graphene
