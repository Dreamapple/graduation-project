/*
 * utils.cpp
 *
 *  Created on: 2016年5月2日
 *      Author: meng
 */

#include "utils.h"
#include <algorithm>

string my_lower(string s){
	string ret(s.length(),0);
	transform(s.begin(),s.end(),ret.begin(),::tolower);
	return ret;
}

string strip(string s, char c){
	auto beg=s.begin(), end=s.end();
	decltype(beg) b,e;
	for(b=beg;b!=end;b++){
		if((*b)==c){
			continue;
		}
		break;
	}
	for(e=end-1;(e+1)!=b;e--){
		if((*e)==c){
			continue;
		}
		break;
	}
	return string(b,e+1);
}

string strip_blank(string s){
	auto beg=s.begin(), end=s.end();
	decltype(beg) b,e;
	for(b=beg;b!=end;b++){
		if((*b)==' '||(*b)=='\t'||(*b)=='\r'||(*b)=='\n'){
			continue;
		}
		break;
	}
	for(e=end-1;(e+1)!=beg;e--){
		if((*e)==' '||(*e)=='\t'||(*e)=='\r'||(*e)=='\n'){
			continue;
		}
		break;
	}
	return string(b,e+1);
}

bool split(string &s,string &out1,string&out2,char sp=' '){
	size_t p=s.find(sp);
	if( p==string::npos){
		return false;
	}
	out1=s.substr(0,p);
	out2=s.substr(p+1,s.length()-(p+1));
	out2=strip(out2, sp);
	return true;
}

bool split3(string &s,string &out1,string&out2,string&out3,char sp=' '){
	string part;
	if(split(s,out1,part, sp)){
		if(split(part,out2,out3)){
			return true;
		}
	}
	return false;
}

