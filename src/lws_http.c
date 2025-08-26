/*
 * LWS HTTP
 *
 * Copyright (C) 2023,2025 Andre Naef
 */


#include <lws_http.h>


#define LWS_HTTP_STATUS_N (3 + 7 + 8 + 24 + 7)


lws_http_status_t lws_http_status[LWS_HTTP_STATUS_N] = {
	/* 1xx [n=3] */
	{
		100,
		lws_string("CONTINUE"),
		lws_string("Continue")
	},
	{
		101,
		lws_string("SWITCHING_PROTOCOLS"),
		lws_string("Switching Protocols")
	},
	{
		102,
		lws_string("PROCESSING"),
		lws_string("Processing")
	},

	/* 2xx [n=7] */
	{
		200,
		lws_string("OK"),
		lws_string("OK")
	},
	{
		201,
		lws_string("CREATED"),
		lws_string("Created")
	},
	{
		202,
		lws_string("ACCEPTED"),
		lws_string("Accepted")
	},
	{
		203,
		lws_string("NON_AUTHORITATIVE_INFORMATION"),
		lws_string("Non-Authoritative Information")
	},
	{
		204,
		lws_string("NO_CONTENT"),
		lws_string("No Content")
	},
	{
		205,
		lws_string("RESET_CONTENT"),
		lws_string("Reset Content")
	},
	{
		206,
		lws_string("PARTIAL_CONTENT"),
		lws_string("Partial Content")
	},

	/* 3xx [n=8] */
	{
		300,
		lws_string("MULTIPLE_CHOICES"),
		lws_string("Multiple Choices")
	},
	{
		301,
		lws_string("MOVED_PERMANENTLY"),
		lws_string("Moved Permanently")
	},
	{
		302,
		lws_string("FOUND"),
		lws_string("Found")
	},
	{
		303,
		lws_string("SEE_OTHER"),
		lws_string("See Other")
	},
	{
		304,
		lws_string("NOT_MODIFIED"),
		lws_string("Not Modified")
	},
	{
		305,
		lws_string("USE_PROXY"),
		lws_string("Use Proxy")
	},
	{
		307,
		lws_string("TEMPORARY_REDIRECT"),
		lws_string("Temporary Redirect")
	},
	{
		308,
		lws_string("PERMANENT_REDIRECT"),
		lws_string("Permanent Redirect")
	},

	/* 4xx [n=24] */
	{
		400,
		lws_string("BAD_REQUEST"),
		lws_string("Bad Request")
	},
	{
		401,
		lws_string("UNAUTHORIZED"),
		lws_string("Unauthorized")
	},
	{
		402,
		lws_string("PAYMENT_REQUIRED"),
		lws_string("Payment Required")
	},
	{
		403,
		lws_string("FORBIDDEN"),
		lws_string("Forbidden")
	},
	{
		404,
		lws_string("NOT_FOUND"),
		lws_string("Not Found")
	},
	{
		405,
		lws_string("METHOD_NOT_ALLOWED"),
		lws_string("Method Not Allowed")
	},
	{
		406,
		lws_string("NOT_ACCEPTABLE"),
		lws_string("Not Acceptable")
	},
	{
		407,
		lws_string("PROXY_AUTHENTICATION_REQUIRED"),
		lws_string("Proxy Authentication Required")
	},
	{
		408,
		lws_string("REQUEST_TIMEOUT"),
		lws_string("Request Timeout")
	},
	{
		409,
		lws_string("CONFLICT"),
		lws_string("Conflict")
	},
	{
		410,
		lws_string("GONE"),
		lws_string("Gone")
	},
	{
		411,
		lws_string("LENGTH_REQUIRED"),
		lws_string("Length Required")
	},
	{
		412,
		lws_string("PRECONDITION_FAILED"),
		lws_string("Precondition Failed")
	},
	{
		413,
		lws_string("CONTENT_TOO_LARGE"),
		lws_string("Content Too Large")
	},
	{
		/* legacy */
		413,
		lws_string("REQUEST_ENTITY_TOO_LARGE"),
		lws_string("Request Entity Too Large")
	},
	{
		414,
		lws_string("URI_TOO_LONG"),
		lws_string("URI Too Long")
	},
	{
		/* legacy */
		414,
		lws_string("REQUEST_URI_TOO_LARGE"),
		lws_string("Request URI Too Large")
	},
	{
		415,
		lws_string("UNSUPPORTED_MEDIA_TYPE"),
		lws_string("Unsupported Media Type")
	},
	{
		416,
		lws_string("RANGE_NOT_SATISFIABLE"),
		lws_string("Range Not Satisfiable")
	},
	{
		417,
		lws_string("EXPECTATION_FAILED"),
		lws_string("Expectation Failed")
	},
	{
		421,
		lws_string("MISDIRECTED_REQUEST"),
		lws_string("Misdirected Request")
	},
	{
		422,
		lws_string("UNPROCESSABLE_CONTENT"),
		lws_string("Unprocessable Content")
	},
	{
		426,
		lws_string("UPGRADE_REQUIRED"),
		lws_string("Upgrade Required")
	},
	{
		429,
		lws_string("TOO_MANY_REQUESTS"),
		lws_string("Too Many Requests")
	},

	/* 5xx [n=7] */
	{
		500,
		lws_string("INTERNAL_SERVER_ERROR"),
		lws_string("Internal Server Error")
	},
	{
		501,
		lws_string("NOT_IMPLEMENTED"),
		lws_string("Not Implemented")
	},
	{
		502,
		lws_string("BAD_GATEWAY"),
		lws_string("Bad Gateway")
	},
	{
		503,
		lws_string("SERVICE_UNAVAILABLE"),
		lws_string("Service Unavailable")
	},
	{
		504,
		lws_string("GATEWAY_TIMEOUT"),
		lws_string("Gateway Timeout")
	},
	{
		505,
		lws_string("HTTP_VERSION_NOT_SUPPORTED"),
		lws_string("HTTP Version Not Supported")
	},
	{
		507,
		lws_string("INSUFFICIENT_STORAGE"),
		lws_string("Insufficient Storage")
	},
};
const int lws_http_status_n = LWS_HTTP_STATUS_N;


lws_http_status_t *lws_find_http_status (int code) {
	int  lower, upper, mid;

	lower = 0;
	upper = lws_http_status_n - 1;
	while (lower <= upper) {
		mid = (lower + upper) / 2;
		if (lws_http_status[mid].code < code) {
			lower = mid + 1;
		} else {
			upper = mid - 1;
		}
	}
	return lower < lws_http_status_n && lws_http_status[lower].code == code ?
			&lws_http_status[lower] : NULL;
}
