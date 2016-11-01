/*
 * http_helper.h
 *
 *  Created on: 2016年3月30日
 *      Author: meng
 */

#include <string>
#include <map>
using namespace std;

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

class Http_handler{
public:
	enum{
		request_head=1,
		response_head,
		response_body_have_length,
		response_body_chuncked_header,
		response_body_chuncked_body,
		// body_parsed,
		unusable=256 //像 image, gif这些不需要分析，直接跳过，只是通过body_len记录长度
	}state; // 1,2,3,4
	uint request_header_len,response_header_len,body_len; //plus \r\n\r\n
	map<string,string> request_header_dict,response_header_dict;
	string request_header_buffer,response_header_buffer,body_buffer,decoded_body_buffer; // decoded_body_buffer is utf-8 encoded
	bool request_header_parsed,response_header_parsed, body_parsed;
	bool chunked;
	bool html_type;
	string file_extention; //such as .jpg

	Http_handler();
	static bool  is_request_head(const uchar* data,int len);
	bool parse_request_head_first_line(string &first_line);
	bool parse_request_head();
	bool parse_response_head_first_line(string &first_line);
	bool parse_response_head();
	bool parse_body();
	string dezip();
	string decode ();

	int push(string s);
	int push(const uchar *data,int len);
	int push(const char *data,int len);
	bool ok();
	void clear();
};

