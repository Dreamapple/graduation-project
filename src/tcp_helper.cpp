/*
 * tcp_helper.cpp
 *
 *  Created on: 2016年3月31日
 *      Author: meng
 */

#include "tcp_helper.h"
#include <arpa/inet.h>
#include <iostream>

TCPkey::TCPkey(in_addr sip,in_addr dip,ushort sp,ushort dp){
	if(dp==80){
		ip1=sip;
		ip2=dip;
		port1=sp;
		port2=dp;
	}else if(sp==80){
		ip1=dip;
		ip2=sip;
		port1=dp;
		port2=sp;
	}else if(sip.s_addr<dip.s_addr){ // no port equal to 80 ?
		ip1=sip;
		ip2=dip;
		port1=sp;
		port2=dp;
	}else{
		cerr<<"sip equal to dip!"<<endl;
	}
}

bool TCPkey::operator<(const TCPkey& rhs)const{
	if (ip1.s_addr != rhs.ip1.s_addr){
		return ip1.s_addr < rhs.ip1.s_addr;
	}if (ip2.s_addr != rhs.ip2.s_addr){
		return ip2.s_addr < rhs.ip2.s_addr;
	}
	if (port1 != rhs.port1){
		return port1 < rhs.port1;
	}
	if (port2 != rhs.port2){
		return port2 < rhs.port2;
	}
	return false; // equal
}

TCPstream::TCPstream()
		:sip(),dip(),sport(0),dport(0),handler(),parser()
{
	cout<<"shouldn\'t come here"<<endl;
}
TCPstream::TCPstream(in_addr srcip,in_addr dstip,unsigned short sp,unsigned short dp, Setting *con)
		:sip(srcip),dip(dstip),sport(sp),dport(dp),handler(),parser(con)
{
		repr(cout);
}

void TCPstream::repr(ostream& out){
	out<<"srcip:"<<inet_ntoa(sip)<<" dstip:"<<inet_ntoa(dip)<<" srcport:"<<sport<<" dstport:"<<dport<<endl;
}
int TCPstream::push(const uchar *data, int len){
	printf("TCPstream::push,%.15s...\n", data);
	if (len == 0){
		return -1;// empty string,establish connection
	}
	handler.push(data, len);
	if (handler.ok()){
		printf("handler->ok()\n");
		if(handler.html_type){
			printf("body is html,parse it");
			parser.parse_and_dump(&handler.request_header_dict,&handler.response_header_dict,handler.decoded_body_buffer);
		}
		clear();
	}
	return 0;
}


int TCPstream::clear(){
	// todo body_buffer to add
	printf("\nTCPstream::clear,");
	size_t old=holder.length();
	printf("request_handler head,dict size %u,buffer size %u,\n",handler.request_header_dict.size(),handler.request_header_buffer.size());
	holder.append(handler.request_header_buffer);
	holder+='\n';
	handler.request_header_buffer.clear();
	printf("response_handler head,dict size %u,buffer size %u,\n",handler.response_header_dict.size(),handler.response_header_buffer.size());
	holder+=handler.response_header_buffer;
	holder+='\n';
	handler.response_header_buffer.clear();
	holder.append(handler.decoded_body_buffer);
	handler.clear();
	printf("holder length %d -> %d\n",old,holder.length());
	return 0;
}

StreamState::StreamState(Identity _id):seq(0),ack_seq(0),id(_id),buffer(){}
bool StreamState::push(TCPhdr *tcp, const uchar *data, uint len){
	if(len==0){
		return false;
	}
	uint _seq = tcp->seq();
	uchar *data_t = new uchar[len];
	memcpy(data_t, data, len);
	_store s={_seq, len, data_t};
	buffer.push_back(s);
	return true;
}
bool StreamState::pop(const uchar *&data,int& len){
	for(auto beg = buffer.begin();beg!=buffer.end();beg++){
		if(beg->seq==seq){
			data = beg->data;
			len = beg->len;
			seq += len;
			return true;
		}
	}
	return false;
}
void StreamState::assert(TCPhdr *tcp){
	// tcp->seq()==seq;
	// tcp->ack_seq()==ack_seq; //todo 分析ack，跟踪服务器/客户端数据包情况
}

ClientStream::ClientStream():StreamState(client),state(closed){}
bool ClientStream::push(TCPhdr *tcp, const uchar *data, uint len){
	if(tcp->dest()==80){
		switch (state){
		case closed:
			if(tcp->syn()){
				state = established;
				seq = tcp->seq()+1;
			}
			break;
		case established:
			if(tcp->fin()){
				state = closed;
			}
			StreamState::push(tcp, data, len);
			break;
		}
		return true;
	}
	return false;
}

ServerStream::ServerStream():StreamState(server),state(closed){}
bool ServerStream::push(TCPhdr *tcp, const uchar *data, uint len){
	if(tcp->source()==80){
		switch (state){
		case closed:
			if(tcp->syn()){
				state = synrecv;
				seq = tcp->seq()+1;
			}
			break;
		case synrecv:
		case established:
			if(tcp->syn()){
				state = established;
			}
			if(len==0){
				break;
			}
			StreamState::push(tcp, data, len);
			if(tcp->fin()){
				state = closed;
			}
			break;

		}
		return true;
	}
	return false;
}

SeqTCPStream::SeqTCPStream(in_addr srcip,in_addr dstip,ushort sp,ushort dp, Setting *conf)
		:TCPstream(srcip, dstip, sp,dp, conf){}
int SeqTCPStream::push( const uchar *data, int len){
	TCPhdr *tcp = (TCPhdr*)data;
	data +=tcp->length();
	len -= tcp->length();
	return push(tcp, data, len);
}
int SeqTCPStream::push(TCPhdr* tcp, const uchar *data, int len){
	if(Client.push(tcp, data, len)){
		while (Client.pop(data, len)){
			TCPstream::push(data, len);
		}
	}else if(Server.push(tcp,data, len)){
		while (Server.pop(data, len)){
			TCPstream::push(data, len);
		}
	}else{
		// wrong!
	}
	return 0;
}


Manager::Manager():id(0),buffer(),conf(0){}

Manager::Manager(Setting *con):id(0),buffer(),conf(con){}

int Manager::feed(const uchar *data, pcap_pkthdr &packet){
	id++;
	printf("[*]Manager::feed 分析第%d个包... \n",id);
    printf("Packet length: %d\n", packet.len);
    string time_str(ctime((const time_t *)&packet.ts.tv_sec));
    time_str.erase(time_str.length()-1);
    printf("Recieved time: %s\n",time_str.c_str() );

	Ether *ether = (Ether *)data;
	data += sizeof(Ether);
	if (ether->type != 8){
		printf("[!] not an ip package!\n");
		return 1;
	}

	IPhdr *ip = (IPhdr*)data;
	data += ip->getHeadLength();
	if (ip->hdr.ip_p != 6){
		printf( "[!]IP protocol id %d unknown!", ip->hdr.ip_p);
		return 2;
	}

	TCPhdr *tcp = (TCPhdr*)data;
	TCPkey key( ip->hdr.ip_src, ip->hdr.ip_dst, tcp->source(), tcp->dest());
	if(buffer.count(key)==0){
		SeqTCPStream *tc=new SeqTCPStream(ip->hdr.ip_src, ip->hdr.ip_dst, tcp->source(), tcp->dest(), conf);
		buffer.insert({key,tc});
	}
	buffer[key]->push(data, ip->getTcpLength());
	printf("[*]Manager::feed 分析第%d个包结束. \n\n",id);
	return 0;
}

void Manager::buffer_out(ostream& of){
	decltype(buffer.begin()) beg = buffer.begin();
	decltype(buffer.end()) end = buffer.end();
	while (beg != end){
		TCPstream *tc=(*beg).second;
		if (1 ||tc->holder.length() > 0){
			tc->repr(of);
			of << endl;
			of << tc->holder << endl;
		}
		beg++;
	}
}

string int2char(in_addr a){
	return string(inet_ntoa(a));
}// test
