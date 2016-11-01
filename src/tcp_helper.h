#include <map>
#include <string>
#include <iostream>
#include <list>
#include <cstring>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pcap.h>
#include "http_helper.h"
#include "html_parser.h"
using namespace std;

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

struct Ether{ // ethernet head
	char dst[6], src[6];
	short type;
};

struct IPhdr{
	ip hdr;
	size_t getHeadLength(){
		return hdr.ip_hl * 4;
	}
	size_t getIPTotalLength(){
		ushort l=hdr.ip_len;
		return ((l&0xff) << 8) +(l>>8);
	}
	size_t getTcpLength(){
		return getIPTotalLength() - getHeadLength();
	}
};

class TCPhdr{
	tcphdr _tcp;
	ushort fix16(ushort p){
		return (p>>8)|((p&0xff)<<8);
	}
	uint fix32(uint p){
		return ((p&0xff000000)>>24)|((p&0x00ff0000)>>8)|((p&0x0000ff00)<<8)|((p&0x000000ff)<<24);
	}
public:
	int length(){
		return _tcp.doff*4;
	}
	ushort source(){
		return fix16(_tcp.source);
	}
	ushort dest(){
		return fix16(_tcp.dest);
	}
	uint seq(){
		return fix32(_tcp.seq);
	}
	uint ack_seq(){
		return fix32(_tcp.ack_seq);
	}
	bool syn(){
		return _tcp.syn;
	}
	bool ack(){
		return _tcp.ack;
	}
	bool fin(){
		return _tcp.fin;
	}

};

class TCPkey{
	in_addr ip1, ip2;
	ushort port1, port2;
public:
	TCPkey(in_addr sip,in_addr dip,ushort sp,ushort dp);
	bool operator<(const TCPkey& rhs)const;
};


class TCPstream{
	in_addr sip,dip;
	ushort sport,dport;
	Http_handler handler;
	HtmlParser parser;
public:
	TCPstream(in_addr srcip,in_addr dstip,ushort sp,ushort dp, Setting *);
	int push(const uchar *data, int len);
	void parse_html();
	int clear();
};


struct StreamState{
	uint seq, ack_seq;
	enum Identity{
		client = 1,
		server = 2,
	} id;
	struct _store{
		uint seq;
		uint len;
		uchar *data;
	};
	list<_store> buffer;
	StreamState(Identity _id);
	bool push(TCPhdr *tcp, const uchar *data, uint len);
	bool pop(const uchar *&data,int& len);
	void assert(TCPhdr *tcp);
};

class ClientStream:public StreamState{
	enum {
		closed,
		//synsent,
		established,
		//finwait,
		//timewait
	} state;
public:
	ClientStream();
	bool push(TCPhdr *tcp, const uchar *data, uint len);
};

class ServerStream:public StreamState{
	enum {
		closed,
		//listen,
		synrecv,
		established,
		//closewait,
		//lastack,
	} state;
public:
	ServerStream();
	bool push(TCPhdr *tcp, const uchar *data, uint len);
};

class SeqTCPStream: public TCPstream{
	ClientStream Client;
	ServerStream Server;
public:
	SeqTCPStream(in_addr srcip,in_addr dstip,ushort sp,ushort dp, Setting *conf);
	int push( const uchar *data, int len);
};

class Manager{
	unsigned int id;
	map<TCPkey, SeqTCPStream*> buffer;
	Setting* conf;
public:
	Manager();
	Manager(Setting*);
	int feed(const uchar *data, pcap_pkthdr &packet);

	void buffer_out(ostream&); // just for test
};
