/*
 *  main.cpp
 *
 *  Created on: 2016年3月26日
 *      Author: meng
 */

#include <iostream>
#include <iomanip>
#include <csignal>
#include <sys/stat.h>

#include "pcap_helper.h"
#include "tcp_helper.h"
#include "html_parser.h"
int signaled = 0;
void my_handler (int param)
{
  signaled = 1;
  if (param==SIGINT){}
}

int main(int argc, char* argv[]){
	printf("%s is running\n",argv[0]);
	ICapture *capture=0;
	if(argc>1){
		struct stat buf;
		if(stat(argv[1], &buf)==0 && S_ISREG(buf.st_mode)){
			capture = new FileCapture(argv[1]);
			printf("开始处理文件：%s\n", argv[1]);
		}
	}
	if (!capture){
		capture = new Capture;
		printf("网络抓包开始\n");
	}
	 Setting *conf = new Setting("config.cfg"); //todo 根据配置启动 -c config.ini
	Manager manager(conf);
	while (!capture->eof()){
		pcap_pkthdr hdr;
		const u_char* data=capture->next(&hdr);
		manager.feed(data, hdr);
	}
	delete capture;
	printf("Finished\n");
}
