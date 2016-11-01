/*
 * html_parser.cpp
 *
 *  Created on: 2016年3月31日
 *      Author: meng
 */

#include "html_parser.h"
#include "http_helper.h"
#include <libconfig.h++>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <ctime>
#include "utils.h"
// using namespace libconfig;

string HtmlParser::clear_mark_up(string& in) {
	if (!conf->strip_off) {
		return in;
	}
	ostringstream out;
	int level = 0;
	auto beg = in.begin(), end = in.end();
	for (; beg != end; beg++) {
		switch (*beg) {
		case '<':
			level++;
			break;
		case '>':
			if (level > 0) {
				level--;
			} else {
				out.str("");
				out.clear();
			}
			break;
		default:
			if (level == 0) {
				out << (*beg);
			}
		}
	}
	return out.str();
}
string clear_mark_up_and_remove_empty(string& in) {
	ostringstream out;
	int level = 0;
	auto beg = in.begin(), end = in.end();
	for (; beg != end; beg++) {
		switch (*beg) {
		case '<':
			level++;
			break;
		case '>':
			if (level) {
				level--;
			}
			break;
		case ' ':
		case '\t':
		case '\n':
		case '\r':
			break;
		default:
			if (level == 0) {
				out << (*beg);
			}
		}
	}
	return out.str();
}

template<typename T>
void Info::dump(T& o) {
	o << endl << "host:" << host << endl;
	o << "url:" << url << endl;
	o << "time:" << time << endl;
	o << "title:" << title << endl;
	o << "subject:" << subject << endl;
	o << "userid:" << userid << endl;
	o << "content:" << content << endl;
}

Setting::Setting() :
		filename(""), config_buffer(), strip_off(false) {
	printf("new Setting\n");
}

Setting::Setting(string file) :
		filename(file), config_buffer(), strip_off(false) {
	printf("[*]new Setting with file name\n");
	libconfig::Config cfg;
	cfg.readFile("config.cfg");
	const libconfig::Setting& root = cfg.getRoot();
	libconfig::Setting& names = root["names"];
	for (int i = 0; i < names.getLength(); ++i) {
		string name = names[i];
		cout << "pattern name: " << name << endl;
		Pattern *p = new Pattern;
		p->name = name;
		const libconfig::Setting& pa = root[name.c_str()];
		pa.lookupValue("segment_key", p->segment_key);
		pa.lookupValue("subject_start", p->subject_start);
		pa.lookupValue("subject_end", p->subject_end);
		pa.lookupValue("userid_start", p->userid_start);
		pa.lookupValue("userid_end", p->userid_end);
		pa.lookupValue("content_start", p->content_start);
		pa.lookupValue("content_end", p->content_end);
		pa.lookupValue("powerd_by", p->powerd_by);
		config_buffer[name] = p;
	}
	root.lookupValue("strip_off_mark_up", strip_off);
	root.lookupValue("out_file_name", out_file_name);
}

Pattern* Setting::getSiteConfig(string site) {
	printf("getSiteConfig(\"%.20s\")...\n", site.c_str());
	site=my_lower(site);
	if (!config_buffer.empty()) { //todo tree serach
		for (auto beg = config_buffer.begin(); beg != config_buffer.end();beg++) {
			// string key = beg->first;
			Pattern* p = beg->second;
			string key = p->powerd_by;
			if(key.empty()){
				key = beg->first;
			}
			if (key.length() <= site.length()
					&& key == site.substr(0, key.length())) {
				printf("Site is %.20s\n", key.c_str());
				return p;
			}
		}
		return 0;
	} else {
		printf("[!]config_buffer not initiallized!\n");
		return NULL;
	}
}

void Setting::addConfig(string s, Pattern* p) {
	config_buffer[s] = p;
}

HtmlParser::HtmlParser() :
		conf(0), conf_t(), req(0), resp(0), start(0) {

}

HtmlParser::HtmlParser(Setting *Setting) :
		conf(Setting), conf_t(), req(0), resp(0), start(0) {
}

void HtmlParser::parse_and_dump(tp_dict* req_, tp_dict* resp_, string& html) {
	InfoList *info = parse(req_, resp_, html);
	if (info != NULL) {
		ofstream out(conf->out_file_name, ios::app | ios::out);
		dump(info, out);
		delete info;
		out.close();
	}
}

template<typename T>
void HtmlParser::dump(InfoList*info, T& o) {
	printf("HtmlParser::dump");
	auto beg = info->begin(), end = info->end();
	for (; beg != end; beg++) {
		(*beg)->dump(o);
	}
}
InfoList* HtmlParser::parse(tp_dict* req_, tp_dict* resp_, string& html) {
	printf("[*]HtmlParser::parse\n");

	if (!conf_t) {
		string site = getsite(html);
		if (site.empty()) {
			site = getsite(req_);
			if (site.empty()) {
				return NULL;
			}
		}
		conf_t = conf->getSiteConfig(site);

		if (!conf_t) {
			return NULL;
		}
	}

	req = req_;
	resp = resp_;
	// html=_html;
	InfoList *ret = new InfoList;
	getTitle(html);
	getSubject(html);
	string segment;
	int id = 1;
	while (getSegment(html, segment, id++)) {
		if (segment.empty()) {
			printf("[!]error! segment length is 0!");
			return 0;
		}
		Info* i = getInformation(segment);
		if (i) {
			ret->push_back(i);
		}
	}
	clear();
	return ret;
}
string HtmlParser::getsite(string& html) {
	printf("HtmlParser::getsite from html),html length is %d\n", html.length());
	// todo:powered by,copyright and so on
	size_t p, q;
	if ((p = html.rfind("Powered by")) != string::npos) {
		q = html.find("</div>", p);
		string subs = html.substr(p + 10, q - p - 10);
		return clear_mark_up_and_remove_empty(subs);
	}
	if ((p = html.rfind("powered by")) != string::npos) {
		q = html.find("</div>", p);
		string subs = html.substr(p + 10, q - p - 10);
		return clear_mark_up_and_remove_empty(subs);
	}
	if ((p = html.rfind("copyright")) != string::npos) {
		q = html.find("</div>", p);
		string subs = html.substr(p, q - p);
		return clear_mark_up_and_remove_empty(subs);
	}
	printf("not found p is %u\n", p);
	return "";
}
string HtmlParser::getsite(tp_dict* req) {
	printf("HtmlParser::getsite from host...");
	if (req == NULL) {
		printf("request header dict is NULL!\n");
		return "";
	}
	if ((*req)["host"] == "bbs.byr.cn") {
		printf("done\n");
		return "nforum";
	} else {
		printf("unknow host %.20s \n ", (*req)["host"].c_str());
		return "";
	}
}
bool HtmlParser::getSegment(string &html, string& s, int id) {
	printf("tmlParser::getSegment,\n");
	string sk = conf_t->segment_key;
	if (start >= html.length()) {
		return false;
	}
	size_t next = html.find(sk, start + sk.length());
	size_t len;
	if (next == string::npos) {
		next = html.length();
		len = next - start;
		s = html.substr(start, next - start);
		start = next;
		printf("segment %d is:%.20s\n", id, s.c_str());
	} else {
		len = next - start + sk.length();
		s = html.substr(start, len);
		start = next + 1;
		printf("segment %d is:%.20s\n", id, s.c_str());
	}
	return true;
}

Info* HtmlParser::getInformation(string& s) {
	printf("HtmlParser::getInformation...\n");
	Info* i = new Info;
	time_t rawtime;
	time(&rawtime);
	i->host = (*req)["host"];
	i->ip = (*req)["ip"]; //todo
	i->url = (*req)["url"]; //todo
	i->time = ctime(&rawtime);
	i->time.erase(i->time.length()-1);
	i->type = 0;
	i->title = title;
	i->subject = subject;

	size_t beg, end;
	string subs;

	if (string::npos != (beg = s.find(conf_t->userid_start))) {
		end = s.find(conf_t->userid_end, beg);
		subs = s.substr(beg, end - beg);
		i->userid = clear_mark_up(subs);
		printf("get userid:%.20s\n", i->userid.c_str());
	}

	if (string::npos != (beg = s.find(conf_t->content_start))) {
		end = s.find(conf_t->content_end, beg);
		subs = s.substr(beg, end - beg);
		i->content = clear_mark_up(subs);
		printf("get content:%.20s\n", i->content.c_str());
	}
	printf("getInformation done\n");
	if (i->content.empty()) {
		printf("content is empty, ignore this info");
		delete i;
		return 0;
	}
	return i;
}

bool HtmlParser::getTitle(const string& html) {
	size_t a = html.find("title");
	size_t b = html.find("/title", a);
	if (a != string::npos && b != string::npos) {
		a += 6;
		title = html.substr(a, b - a - 1);
		return true;
	}
	return false;
}

bool HtmlParser::getSubject(const string& html) {
	size_t beg, end;
	if (string::npos != (beg = html.find(conf_t->subject_start))) {
		end = html.find(conf_t->subject_end, beg);
		string subs = html.substr(beg, end - beg);
		subject = clear_mark_up(subs);
		printf("get subject:%.20s\n", subject.c_str());
		return true;
	}
	return false;
}

void HtmlParser::clear() {
	req = resp = NULL;

}

// just for test
Setting *setConfig() {
	Setting *con = new Setting;
	Pattern *d = new Pattern;
	Pattern *dd = new Pattern;
	d->segment_key = "a-content-wrap";
	d->subject_start = "标&nbsp;&nbsp;题";
	d->subject_end = "<br />";
	d->userid_start = "发信人";
	d->userid_end = "信区";
	d->content_start = "发信站";
	d->content_end = "※ 来源";
	string k = "nforum";
	con->addConfig(k, d);
	con->addConfig("other", dd);
	return con;
}
