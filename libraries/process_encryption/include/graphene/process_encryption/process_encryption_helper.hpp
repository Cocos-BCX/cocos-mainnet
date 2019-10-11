#pragma once

namespace graphene
{
namespace process_encryption
{
struct process_variable
{
   std::vector<uint64_t> random;
   std::vector<uint64_t> timetable;
   int16_t next_random_index()
   {
      auto temp = random_index++;
      return temp;
   }
   int16_t next_time_index()
   {
      auto temp = time_index++;
      return temp;
   }
   void reset_index()
   {
      random_index = 0;
      time_index = 0;
   }

private:
   uint16_t random_index = 0;
   uint16_t time_index = 0;
};
class process_encryption_helper
{
   char a[64];
public:
   process_encryption_helper(){};
   process_encryption_helper(std::string chain, std::string prefix, fc::time_point_sec time);
   void decrypt(const std::vector<char> &process_value, process_variable &_process_value);
   std::vector<char> encrypt(const process_variable &_process_value);
};

} // namespace process_encryption
} // namespace graphene
FC_REFLECT(graphene::process_encryption::process_variable,
           (random)(timetable))