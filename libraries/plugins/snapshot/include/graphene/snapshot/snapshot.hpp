/*
 * Copyright (c) 2017 Peter Conrad, and contributors.
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

#include <graphene/app/plugin.hpp>
#include <graphene/chain/database.hpp>

#include <fc/time.hpp>

namespace graphene { namespace snapshot_plugin {

class snapshot_plugin : public graphene::app::plugin {
   public:
      ~snapshot_plugin() {}

      std::string plugin_name()const override;
      std::string plugin_description()const override;

      virtual void plugin_set_program_options(
         boost::program_options::options_description &command_line_options,
         boost::program_options::options_description &config_file_options
      ) override;

      virtual void plugin_initialize( const boost::program_options::variables_map& options ) override;
      virtual void plugin_startup() override;
      virtual void plugin_shutdown() override;

   private:
       void check_snapshot( const graphene::chain::signed_block& b);

       uint32_t           snapshot_block = -1, last_block = 0;
       fc::time_point_sec snapshot_time = fc::time_point_sec::maximum(), last_time = fc::time_point_sec(1);
       fc::path           dest;
};

} } //graphene::snapshot_plugin
