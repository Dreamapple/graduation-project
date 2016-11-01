/*
 * http_helper.cpp
 *
 *  Created on: 2016年3月28日
 *      Author: meng
 */

//#include "http_helper.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include "tcp_helper.h"
#include "zstream.h"
#include <chardet.h>
#include <iconv.h>
#include "utils.h"

void dump_file(string s,const char tp[]){
	printf("%s\n","dump");
	static int id=0;
	char name[20];
	sprintf(name,"./dump/%d.%s",id++,tp);
	cout<<name<<" ...file length is "<<s.length();
	ofstream f(name,ios::binary);
	f<<s;
	f.close();
	cout<<"done"<<endl;
	return;
}

void dump_file(string s,string t){
	static int id=0;
	char name[200];
	sprintf(name,"./dump/%d.%s",id++,t.c_str());
	printf("dump %s and length is %u...",name, s.length());
	ofstream f(name,ios::binary);
	f<<s;
	f.close();
	cout<<" done"<<endl;
	return;
}

char * GetLocalEncoding(const char* in_str, unsigned int str_len){
    chardet_t chardect=NULL;
    char *out_encode=new char[CHARDET_MAX_ENCODING_NAME];
    if(chardet_create(&chardect)==CHARDET_RESULT_OK){
        if(chardet_handle_data(chardect, in_str, (unsigned int)str_len) == CHARDET_RESULT_OK){
            if(chardet_data_end(chardect) == CHARDET_RESULT_OK){
                chardet_get_charset(chardect, out_encode, CHARDET_MAX_ENCODING_NAME);
            }
        }
    }

    if(chardect){
        chardet_destroy(chardect);
        return out_encode;
    }
    else{
    	return NULL;
    }

}

Http_handler::Http_handler(){
	clear();
}
int Http_handler::push(const unsigned char *data, int len){
	return push((const char *)data, len);
}

int Http_handler::push(const char *data, int len){
	const char *sub;
	int len_t;

	switch(state){
	case request_head:
		printf("Http_handler state is request_head\n" );
		request_header_buffer.append(data,len);
		request_header_parsed=parse_request_head();
		if(request_header_parsed){
			state=response_head;
		}
		break;
	case response_head:
		printf("Http_handler state is response_head\n" );
		sub=strstr((const char*)data,"\r\n\r\n");
		if(sub!=NULL){
			sub+=4;
			len_t=sub-data;
			response_header_len+=len_t; // response_header_len 必需先初始化为0 // 现在已经没有用了
			response_header_buffer.append(data,len_t);
			if((response_header_parsed=parse_response_head())==true){
				// state=response_body_have_length; //head结束，应该来body了 //让 parse 去改变状态
				data=sub;
				len-=len_t;
				if(len>0){
					push(data,len);
				}
			}
		}else{ //sub==NULL
			//两种情况，一是请求格式不标准，是以\n\n结尾；二是头部太长，分开几部分传输
			response_header_buffer.append(data,response_header_len); //只考虑第二种情况
			return 0;
		}
		if (len==0){
			return 0;
		}
		break;
	case response_body_have_length:
		printf("Http_handler state is response_body\n" );
		if(body_parsed){
			printf("[!]error state!");
			clear();
			push(data,len);
		}
		if(len>0 && !chunked){  //chunked 在parse_response_head()中分析
			printf("not chunked,data length is %d\n",len);
			body_buffer.append(data, len);
			if(body_buffer.length()==body_len){ //body_len 在parse_response_head()中分析
				decoded_body_buffer=dezip();
				dump_file(decoded_body_buffer,file_extention); //todo remove
				decoded_body_buffer=decode();
				body_parsed=true;
			}
		}
		break;
	case response_body_chuncked_header:
		printf("Http_handler state is response_body_chuncked_header\n" );
		if(len==0){
			printf("data length is 0, end\n"); //两难抉择，浏览器会先加载一部分内容，等之后的数据包到达
			//可是我们的分析程序会面临重复分析，状态不确定的问题
			//dump_file(decoded_body_buffer,file_extention); //
			// decoded_body_buffer=decode();
			// body_parsed=true; //为真，会调用clear
			return 0; //chunk end
		}
		sub=strstr(data,"\r\n");
		if(sub==NULL){
			printf("[!] can not file \\r\\n in chunk body. this is not chunked?\n");
			clear();
			return -1;
		}
		if(1==sscanf(data,"%x",&len_t)){
			printf("chunked data,data length is %d\n",len_t);
			if(len_t==0){
				printf("this chunk length is 0, end\n");
				dump_file(decoded_body_buffer, file_extention);
				if(html_type){
					decoded_body_buffer=decode();
					body_parsed=true;
				}else{
					state=request_head; //
				}
				return 0; //chunk end
			}
			else{
				sub+=2;
				len-=(sub-data);
				body_len=len_t;
				state=response_body_chuncked_body;
				return push(sub,len);
			}
		}
		else{
			printf("[!]chunk parse error!");
			clear();
			return -1;
		} // sscanf if end
		break;
	case response_body_chuncked_body:
		printf("Http_handler state is response_body_chuncked_body, chunk length is %d, data length is %d, buffer already have %d\n", body_len,len,body_buffer.length() );
		if(len+body_buffer.length()<=body_len){ //这个body_len在上面分析
			body_buffer.append(data,len);
			return 0;
		}
		else{
			printf("[*]this chunk full!\n");
			len_t=body_len-body_buffer.length();
			body_buffer.append(data, len_t);
			decoded_body_buffer+=dezip();

			// dump_file(decoded_body_buffer,file_extention); //todo remove
			// decoded_body_buffer=decode();
			// body_parsed=true;

			len_t+=2;
			data+=len_t;
			len-=len_t;
			state=response_body_chuncked_header;
			push(data,len);
		}
		break;
	case unusable:
		printf("Http_handler state is unusable\n" );
		break;

	} //switch end
	return 0;
}

//parse first line such as HTTP/1.1 200 OK
bool Http_handler::parse_response_head_first_line(string &first_line){
	printf("firstline is %s\n", first_line.c_str());
	string protocal,status_code,status_msg;
	if(split3(first_line, protocal,status_code,status_msg, ' ')){
		response_header_dict["protocal"]=protocal;
		response_header_dict["status_code"]=status_code;
		response_header_dict["status_msg"]=status_msg;

		printf("match first line succeed, protocal: \"%s\",status_code: \"%s\",status_msg: \"%s\"\n", protocal.c_str(),status_code.c_str(),status_msg.c_str());
		if(response_header_dict["status_code"]!="200"){
			printf("[!] 不是200 状态\n");
			clear();
			return false;
		}
	}
	return true;
}
bool Http_handler::parse_response_head(){
	printf("Http_handler::parse_response_head\n");
	string ender="\r\n", firstline;
	size_t end=response_header_buffer.find(ender);
	if(end==string::npos){
		return false;
	}
	firstline=response_header_buffer.substr(0,end);
	if(!parse_response_head_first_line(firstline)){
		return true; //parsed
	}
	end+=2;
	//parse attributes such as:
	//content-encoding: gzip
	//content-length: 8081
	size_t p=end,n=end+1;
	while(p<response_header_len){
		if((n=response_header_buffer.find(ender,p))!= string::npos){
			if(n==p){
				printf("[*]response head parse finished.\n");
				break;
			}
			n+=ender.length();
			string subs=response_header_buffer.substr(p,n-p);
			string r1,r2;
			if(split(subs,r1,r2,':')){
				response_header_dict[r1]=r2;
				printf("[*]response head line \"%s\": \"%s\"\n",r1.c_str(),r2.c_str());
			}else{
				printf("[!]meet unexpented head str \"%s\"\n",subs.c_str());
			}
			p=n;
		}
	}
		/////////////////////////////
	if(!response_header_dict["transfer-encoding"].empty()&&response_header_dict["transfer-encoding"]=="chunked"){
		chunked=true;
		state=response_body_chuncked_header;
	}else{
		body_len=atoi(response_header_dict["content-length"].c_str());
		state=response_body_have_length;
	}

	string content_type=response_header_dict["content-type"]; //content_type=" text/html;charset=GBK";
	string r;
	split(content_type,r,file_extention,'/');
	if(content_type.substr(0,9)=="text/html"){
		html_type=true;
	}
	else{
		html_type=false;
		// state=unusable;
	}
	return true;
}



bool Http_handler::parse_body(){
	if(!body_len){
		return false;
	}
	decoded_body_buffer=dezip();
	dump_file(decoded_body_buffer,file_extention);
	decoded_body_buffer=decode();
	//clear(); let tcp helper to clear.
	return true;
}

string Http_handler::dezip(){
	string &s=body_buffer;
	char *rawdata;int nrawdata=-1;
	printf("dezipping(content_encoding is %s)...",response_header_dict["content-encoding"].c_str());
	const char *data=s.c_str();
	string content_encoding=response_header_dict["content-encoding"];
	if(content_encoding.substr(0,4)=="gzip"){
		nrawdata =  body_len * 16 + Z_HEADER_SIZE;
		rawdata=new char[nrawdata];
		if((httpgzdecompress((Bytef *)data, body_len,  (Bytef *)rawdata, (uLong *)&nrawdata)) == 0){
		}
	}else if(content_encoding.substr(0,7)=="deflate"){
		nrawdata =  body_len * 16 + Z_HEADER_SIZE;
		rawdata=new char[nrawdata];
		if((zdecompress((Bytef *)data, body_len,  (Bytef *)rawdata, (uLong *)&nrawdata)) == 0){
		}
	}else if(content_encoding==""){
		rawdata=new char[body_len];
		memcpy(rawdata,data,body_len);
		nrawdata=body_len;
	}
	cout<<"done!nrawdata "<<nrawdata<<endl;
	return string(rawdata,nrawdata);
}


string Http_handler::decode(){
	if(!html_type){
		return decoded_body_buffer;
	}
	string &s=decoded_body_buffer;
	 const char *txtdata=s.c_str();int  ntxtdata=s.length();
	 char *todata;
	 int ntodata;
	const char *from_charset=GetLocalEncoding(txtdata,ntxtdata);
	const char *to_charset="utf-8";
	iconv_t cd=NULL;

	char  *p = NULL, *ps = NULL, *outbuf = NULL;

	size_t ninbuf = 0, noutbuf = 0, n = 0;
	if(strcasecmp(from_charset, to_charset) != 0&& (cd = iconv_open(to_charset, from_charset)) != (iconv_t)-1)
	{
		p =(char  *) txtdata;
		ninbuf = ntxtdata;
		n = noutbuf = ninbuf * 8;
		if((ps = outbuf = (char *)calloc(1, noutbuf)))
		{
			if(iconv(cd, &p, &ninbuf, &ps, (size_t *)&n) == (size_t)-1)
			{
				free(outbuf);
				outbuf = NULL;
			}
			else
			{
				noutbuf -= n;
				todata = outbuf;
				ntodata = noutbuf;
				fprintf(stdout, "%s::%d ndata:%d\r\n", __FILE__, __LINE__, ntodata);
			}
		}
		iconv_close(cd);
		return string(todata,ntodata);
	}else{
		return s;
	}
}

void Http_handler::clear(){
	state=request_head; // 1,2,3
	request_header_len=response_header_len=body_len=0;
	request_header_dict.clear();
	response_header_dict.clear();
	request_header_buffer="";
	response_header_buffer="";
	body_buffer="";
	decoded_body_buffer="";
	request_header_parsed=response_header_parsed=body_parsed=chunked=html_type=false;
}
bool Http_handler::parse_request_head_first_line(string &first_line){
	string method,uri,protocal;
	if(split3(first_line,method,uri,protocal, ' ')){
		printf("first line is %s,method:%s,uri%s,protocal:%s\n",first_line.c_str(),method.c_str(),uri.c_str(),protocal.c_str());
		request_header_dict["method"]=method;
		request_header_dict["url"]=uri;
		request_header_dict["protocal"]=protocal;
		return true;
	}
	return false;
}

//todo add comment of param and return meaning
bool Http_handler::parse_request_head(){
	cout<<"parse_request_head"<<endl;
	// GET http://www.cnblogs.com/ HTTP/1.1
	string&s = request_header_buffer;
	string ender;
	size_t t=s.find('\n')+1;
	if(s.find('\r')==string::npos){
		ender="\n";
	}else{
		ender="\r\n";
	}
	string firstline=s.substr(0,t-ender.length());
	if (!parse_request_head_first_line(firstline)){
		return false;
	}
	size_t p=t,n=t;
	string rr1,rr2;
	string sub;
	int id=1;
	while(p<s.length()){
		if((n=s.find("\r\n",p))==string::npos){
			break;
		}
		sub=s.substr(p,n-p);
		if(sub.length()==0){
			break;
		}
		if(split(sub, rr1,rr2,':')){
			rr1=strip_blank(my_lower(rr1));
			rr2=strip_blank(my_lower(rr2));
			request_header_dict[rr1]=rr2;
			printf("split succeed,\"%s\":\"%s\"\n",rr1.c_str(),rr2.c_str());
		}else{
			cerr<<"split failed: "<<sub<<endl;
			// break;
		}
		p=n+2;
		id++;
	}
	return true;
}

bool Http_handler::ok(){
	return body_parsed;
}

bool Http_handler::is_request_head(const uchar *data, int len){
	string s((const char*)data,(size_t)4);
	string b="GET ";
	string c="POST";
	return (s==b)||(s==c);
}

