## Code Structure

The code is structured as follows:

| File                  | Description                                  |
| --------------------- | -------------------------------------------- |
| `lws_runtime.{h,c}`   | LWS runtime for AWS Lambda, context, logging |
| `lws_interface.{h,c}` | AWS Lambda runtime interface                 |
| `lws_request.{h,c}`   | Request processing logic                     |
| `lws_state.{h,c}`     | Lua state management                         |
| `lws_lib.{h,c}`       | Lua library                                  |
| `lws_table.{h,c}`     | Hash table                                   |
| `lws_codec.{h,c}`     | Base64 and UTF-8 processing                  |
| `lws_http.{h,c}`      | HTTP statuses                                |
| `lws_ngx.{h,c}`       | NGINX-derived structures and functions       |
