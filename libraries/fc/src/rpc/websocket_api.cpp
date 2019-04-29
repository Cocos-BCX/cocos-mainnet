
#include <fc/rpc/websocket_api.hpp>

namespace fc
{
namespace rpc
{

websocket_api_connection::~websocket_api_connection()
{
}

websocket_api_connection::websocket_api_connection(fc::http::websocket_connection &c)
    : _connection(c)
{
      //  websocket_api_connection 构造函数 ，初始化 call ,  notice , callback 方法回调
      _rpc_state.add_method("call", [this](const variants &args) -> variant {
            /*nico start*/
            FC_ASSERT(args.size() == 3 && args[2].is_array());     //验证参数个数一定为3个，并且第三个参数为数组 ，第二个参数为方法名。
            FC_ASSERT(args[0].is_string() || args[0].is_uint64()); //验证第一个参数，通常为API接口名或API ID
            FC_ASSERT(args[1].is_string());                        //验证第二个参数 方法名一定为字符串
            api_id_type api_id;
            if (args[0].is_string()) //验证第一个参数是否是字符串   ,通常为database , network_broadcast,  history 等API 接口名
            {
                  variant subresult = this->receive_call(1, args[0].as_string()); //根据对应的API 接口名向用户返回对应的API ID
                  if (args[1].as_string() == "")
                        return subresult;
                  api_id = subresult.as_uint64();
            }
            else
                  api_id = args[0].as_uint64();

            return this->receive_call(
                api_id,
                args[1].as_string(),
                args[2].get_array());
            /*nico end*/
            /*  BM 
      FC_ASSERT( args.size() == 3 && args[2].is_array() );
      api_id_type api_id;
      if( args[0].is_string() )
      {
         variant subresult = this->receive_call( 1, args[0].as_string() );
         api_id = subresult.as_uint64();
      }
      else
         api_id = args[0].as_uint64();

      return this->receive_call(
         api_id,
         args[1].as_string(),
         args[2].get_array() );
      */
      });

      _rpc_state.add_method("notice", [this](const variants &args) -> variant {
            FC_ASSERT(args.size() == 2 && args[1].is_array());
            this->receive_notice(args[0].as_uint64(), args[1].get_array());
            return variant();
      });

      _rpc_state.add_method("callback", [this](const variants &args) -> variant {
            FC_ASSERT(args.size() == 2 && args[1].is_array());
            this->receive_callback(args[0].as_uint64(), args[1].get_array());
            return variant();
      });

      _rpc_state.on_unhandled([&](const std::string &method_name, const variants &args) {
            return this->receive_call(0, method_name, args);
      });

      _connection.on_message_handler([&](const std::string &msg) { on_message(msg, true); });
      _connection.on_http_handler([&](const std::string &msg) { return on_message(msg, false); });
      _connection.closed.connect([this]() { closed(); });
}

variant websocket_api_connection::send_call(
    api_id_type api_id,
    string method_name,
    variants args /* = variants() */)
{
      auto request = _rpc_state.start_remote_call("call", {api_id, std::move(method_name), std::move(args)});
      _connection.send_message(fc::json::to_string(request));
      return _rpc_state.wait_for_response(*request.id);
}

variant websocket_api_connection::send_callback(
    uint64_t callback_id,
    variants args /* = variants() */)
{
      auto request = _rpc_state.start_remote_call("callback", {callback_id, std::move(args)});
      _connection.send_message(fc::json::to_string(request));
      return _rpc_state.wait_for_response(*request.id);
}

void websocket_api_connection::send_notice(
    uint64_t callback_id,
    variants args /* = variants() */)
{
      fc::rpc::request req{optional<uint64_t>(), "notice", {callback_id, std::move(args)}};
      _connection.send_message(fc::json::to_string(req));
}

std::string websocket_api_connection::on_message(
    const std::string &message,
    bool send_message /* = true */)
{
      try
      {
            auto var = fc::json::from_string(message); //   websocket_api 普通消息解包 ，string 转 json 对象
            const auto &var_obj = var.get_object();

            if (var_obj.contains("method"))
            {
                  auto call = var.as<fc::rpc::request>(); //   普通消息解包 json 转requset 对象
                  exception_ptr optexcept;
                  try
                  {
                        try
                        {
#ifdef LOG_LONG_API
                              auto start = time_point::now();
#endif
                              //wdump((call.method)(call.params)); //nico API 参数
                              auto result = _rpc_state.local_call(call.method, call.params); //  API函数调用

#ifdef LOG_LONG_API
                              auto end = time_point::now();

                              if (end - start > fc::milliseconds(LOG_LONG_API_MAX_MS))
                                    elog("API call execution time limit exceeded. method: ${m} params: ${p} time: ${t}", ("m", call.method)("p", call.params)("t", end - start));
                              else if (end - start > fc::milliseconds(LOG_LONG_API_WARN_MS))
                                    wlog("API call execution time nearing limit. method: ${m} params: ${p} time: ${t}", ("m", call.method)("p", call.params)("t", end - start));
#endif

                              if (call.id)
                              {
                                    auto reply = fc::json::to_string(response(*call.id, result, "2.0"));
                                    if (send_message)
                                          _connection.send_message(reply);
                                    return reply;
                              }
                        }
                        FC_CAPTURE_AND_RETHROW((call.method)(call.params))
                  }
                  catch (const fc::exception &e)
                  {
                        wlog("websocket api exception :${e}",("e",e));
                        if (call.id)
                        {
                        auto reply = fc::json::to_string(response(*call.id, error_object{*call.id, e.to_string()}, "2.0"));
                        if (send_message)
                              _connection.send_message(reply);

                        return reply;
                        }
                  }
            }
            else
            {
                  auto reply = var.as<fc::rpc::response>();
                  _rpc_state.handle_reply(reply);
            }
      }
      catch (const fc::exception &e)
      {
            wdump((e.to_detail_string()));
            return e.to_detail_string();
      }
      return string();
}

} // namespace rpc
} // namespace fc
