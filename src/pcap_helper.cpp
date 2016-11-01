/*
 * html_parser.cpp
 *
 *  Created on: 2016年3月26日
 *      Author: meng
 */

#include "pcap_helper.h"
#include <csignal>
FileCapture::FileCapture(string file){
	i = ifstream(file, ios::binary);
	if (!i){
		cerr << file << " can not open!" << endl;
		exit(10);
	}
	i.seekg(0, ios::end);
	length = i.tellg();
	i.seekg(0, ios::beg);

	i.read((char *)&header, sizeof(header));
}

bool FileCapture::eof(){
	int p=i.tellg();
	return p>=length;
}

const uchar * FileCapture::next(pcap_pkthdr * h){
	i.read((char *)h, sizeof(pcap_pkthdr));
	u_char* t = new u_char[h->caplen];
	i.read((char *)t, h->caplen);
	return t;
}
Capture* Capture::instance=0;
bool Capture::running=false;
Capture::Capture(){
	*errBuf = 0; // 减少错误信息
	running = true;
	if (instance){
		*this = *instance;
		return;
	}
	devStr= pcap_lookupdev(errBuf);
	if(!devStr){
	  error();
	}
	device = pcap_open_live(devStr, 65535, 1, 0, errBuf);
	if(!device)  {
		error();
	}
	setfilter("port 80");

	instance = this;
	signal(SIGINT, sig_handler);
}

void Capture::sig_handler(int para){
	printf("[!]SIGINT catch, pargrom will exit.\n");
	running = false;
}

void Capture::error(){
	printf("error: %s\n", errBuf);
	exit(1);
}

/*int pcap_compile(pcap_t * p, struct bpf_program * fp, char * str, int optimize, bpf_u_int32 netmask)
			fp：这是一个传出参数，存放编译后的bpf
			str：过滤表达式
			optimize：是否需要优化过滤表达式
			metmask：简单设置为0即可
		  */
void Capture::setfilter(string s){
	  pcap_compile(device, &filter, s.c_str(), 1, 0);
	  pcap_setfilter(device, &filter);
}

const uchar *Capture::next(pcap_pkthdr *packet){
		return  ::pcap_next(device, packet);
}

Capture::~Capture(){
	pcap_close(device);
}

bool Capture::eof(){
	return !running;
}
