/*
 * html_parser.h
 *
 *  Created on: 2016年3月30日
 *      Author: meng
 */

#ifndef HTML_PARSER_H_
#define HTML_PARSER_H_
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
using namespace std;

typedef map<string,string> tp_dict,*pdict;

struct Pattern{
	string name;
	string segment_key;
	string subject_start;
	string subject_end;
	string userid_start;
	string userid_end;
	string content_start;
	string content_end;
	string powerd_by;
};

typedef map<string,Pattern*>  Config_dict;

class Setting{
public:
	string filename;
	Config_dict config_buffer;
	bool strip_off; //把<...>去掉
	string out_file_name;
public:
	Setting();
	Setting(string file);
	Pattern* getSiteConfig(string site);
	void addConfig(string  s,Pattern* p);
};

struct Info{
	string host,url;
	string ip;
	string time;
	int type;

	string title,subject,userid, content;
	template<typename T>void dump(T& o);
};
typedef vector<Info*> InfoList;

struct HtmlParser
{
    Setting *conf; //global
    Pattern *conf_t; //specific for a site
    tp_dict *req,*resp;
    string title, subject; //temp private,duplicate?
    size_t start;
    HtmlParser();
    HtmlParser(Setting *config);
    InfoList* parse(tp_dict* req_,tp_dict* resp_,string& html);
    string getsite(string& html);
    string getsite(tp_dict* req);
    bool getSegment(string&in,string&out,int id=0);
    Info* getInformation(string&);
    bool getTitle(const string& html);
    bool getSubject(const string& html);

    void parse_and_dump(tp_dict* req_,tp_dict* resp_,string& html);
    template<typename T>void dump(InfoList*info, T& out);
    void clear();
    string clear_mark_up(string& in);

};



#endif /* HTML_PARSER_H_ */
