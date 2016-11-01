/*
 * utils.h
 *
 *  Created on: 2016年5月2日
 *      Author: meng
 */

#ifndef UTILS_H_
#define UTILS_H_

#include <string>
using namespace std;

string my_lower(string s);

string strip_blank(string s);

bool split(string &s,string &out1,string&out2,char sp);

bool split3(string &s,string &out1,string&out2,string&out3,char sp);
#endif /* UTILS_H_ */
