#include "pch.h"
#include "Address.h"
#include "AI_hook.h"
#include <timeapi.h>
#include <vector>
#include <fstream>
#include <format>
#include <chrono>
#undef max
#undef min
typedef DWORD Player;
const DWORD CKEY_Z = 1;
const DWORD CKEY_X = 2;
const DWORD CKEY_C = 4;
const DWORD CKEY_SHIFT = 8;
const DWORD CKEY_UP = 16;
const DWORD CKEY_DOWN = 32;
const DWORD CKEY_LEFT = 64;
const DWORD CKEY_RIGHT = 128;
const DWORD* pControl_Key = (DWORD*)(0x52A64C);

const DWORD p_cave = 0x00420754;

DWORD* const pSeedANM = (DWORD*)0x004D4978;
DWORD* const pSeedREP = (DWORD*)0x004D4980;

#include<winsock2.h>
#include <Ws2tcpip.h>
#include <iostream>
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"winmm.lib")

enum Check:WORD
{
	NAK = 0, ACK = 1,  DATA = 2
};

struct DataToSend
{
	DWORD control = 0;
	int time = 0;
	Check check = Check::DATA;
	DataToSend(DWORD c=0, int t=0, Check ck = Check::DATA) :control(c), time(t), check(ck){};
};

struct Settings
{
	union
	{
		sockaddr_in6 addr_self;
		sockaddr_in addr_self4;
	};
	union
	{
		sockaddr_in6 addr_other;
		sockaddr_in addr_other4;
	};
	char ip_Self[128] = { 0 };
	char ip_Other[128] = { 0 };
	int port_self = 0;
	int port_other = 0;
	int delay_frame = 1;
	//int jump_frame = 2;
	int time_out = 60;
	bool is_main = false;
	DWORD rnd_seed;


	SOCKET udp_socket;

	int time_now;
	int real_time_now;
	std::vector<DataToSend> data_self;
	std::vector<DataToSend> data_other;

	int wait_time;
	int updates=0;
	int lasterr = 0;
	int err_occurred=0;

	bool is_debug = true;
	std::fstream log_file;
	std::fstream control_file;
	bool is_save_control = true;
	bool is_blocking = false;
	int slp_time = 1;
	int nak_time=0;

	LARGE_INTEGER timer_freq;
	bool is_ipv6=true;
};

void __fastcall M_Init();

void InfoLog(Settings *ps,const std::string& log)
{
	if (ps->is_debug)
	{
		ps->log_file<<"[INFO]" << log<<"\n";
		std::cout <<"[INFO]" << log<<"\n";
		ps->log_file.flush();
	}
}
void ErrorLog(Settings* ps,const std::string& log,int errCode=0,bool msgBox=true)
{
	if (ps->is_debug)
	{
		if (errCode != 0)
		{
			ps->log_file << "[ERROR]" << log <<", code:"<< errCode << "\n";
			std::cout << "[ERROR]" << log << ", code:" << errCode<< "\n";
		}else{
			ps->log_file << "[ERROR]" << log << "\n";
			std::cout << "[ERROR]" << log << "\n";
		}
		ps->log_file.flush();
	}
	if(msgBox)
		MessageBoxA(NULL, log.c_str(), "ERROR", MB_OK);
}
bool Init()
{
	Settings* pSetting = new Settings();
	Settings& setting=*pSetting;
	Address<Settings*> ns(p_cave);//a code cave
	ns.SetValue(pSetting);
	// read settings
	GetPrivateProfileStringA("network","IPV6_ADDR_SELF","::1", setting.ip_Self,sizeof(setting.ip_Self), ".\\net.ini");
	GetPrivateProfileStringA("network", "IPV6_ADDR_OTHER", "::1", setting.ip_Other, sizeof(setting.ip_Other), ".\\net.ini");
	char buf[128] = { 0 };
	GetPrivateProfileStringA("network", "IPV6_PORT_SELF", "0", buf, sizeof(buf), ".\\net.ini");
	setting.port_self =atoi(buf);
	GetPrivateProfileStringA("network", "IPV6_PORT_OTHER", "0", buf, sizeof(buf), ".\\net.ini");
	setting.port_other = atoi(buf);
	GetPrivateProfileStringA("network", "DELAY_FRAME", "1", buf, sizeof(buf), ".\\net.ini");
	setting.delay_frame = atoi(buf);
	GetPrivateProfileStringA("network", "TIME_OUT", "60", buf, sizeof(buf), ".\\net.ini");
	setting.time_out = atoi(buf);
	GetPrivateProfileStringA("network", "IS_MAIN", "0", buf, sizeof(buf), ".\\net.ini");
	setting.is_main = (atoi(buf))==1;
	GetPrivateProfileStringA("network", "IS_DEBUG", "1", buf, sizeof(buf), ".\\net.ini");
	setting.is_debug = (atoi(buf)) == 1;
	GetPrivateProfileStringA("network", "IS_BLOCKING", "0", buf, sizeof(buf), ".\\net.ini");
	setting.is_blocking = (atoi(buf)) == 1;
	GetPrivateProfileStringA("network", "IS_SAVE_CONTROL", "1", buf, sizeof(buf), ".\\net.ini");
	setting.is_save_control = (atoi(buf)) == 1;
	//GetPrivateProfileStringA("network", "JUMP_FRAME", "2", buf, sizeof(buf), ".\\net.ini");
	//setting.jump_frame = atoi(buf);
	GetPrivateProfileStringA("network", "RNG", "0", buf, sizeof(buf), ".\\net.ini");
	setting.rnd_seed = atoi(buf);

	if (setting.is_debug)
	{
		AllocConsole();
#pragma warning(push)
#pragma warning(disable:4996)
		freopen("CONOUT$", "w", stdout);
#pragma warning(pop)
		setting.log_file.open("./debug.log", std::ios::out | std::ios::ate);
		std::ios::sync_with_stdio(false);
		InfoLog(pSetting, std::format("debug mode on, time: {}\n", std::chrono::system_clock::now()));
	}
	if (setting.is_save_control)
	{
		setting.control_file.open("./control_rep.log", std::ios::out | std::ios::ate);
	}

	if (setting.port_self == 0)
	{
		ErrorLog(pSetting, "fail to read ports in net.ini");
		return false;
	}
	if (setting.port_other == 0)
	{
		InfoLog(pSetting,"use client/host mode");
		MessageBoxA(NULL, "currently using the host/client mode, and here is host.","Info",MB_OK);
	}
	if (setting.delay_frame < 0)
		setting.delay_frame = 0;
	
	// udp
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		ErrorLog(pSetting, "fail to init wsa",WSAGetLastError());
		return false;
	}
	if (strstr(setting.ip_Self, ":") != NULL || strstr(setting.ip_Other,":")) //ipv6
	{
		setting.addr_other = { AF_INET6, htons(setting.port_other) };
		inet_pton(AF_INET6, setting.ip_Other, &(setting.addr_other.sin6_addr));
		setting.addr_self = { AF_INET6, htons(setting.port_self) };

		setting.udp_socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		int res = bind(setting.udp_socket, (SOCKADDR*)&(setting.addr_self), sizeof(setting.addr_self));
		if (res != 0)
			ErrorLog(pSetting, "fail to create socket", WSAGetLastError());
		InfoLog(pSetting, std::format("ipv6 connection, {} to {};port:{},{}", setting.ip_Self, setting.ip_Other, setting.port_self, setting.port_other));
		setting.is_ipv6 = true;
	}else{//ipv4
		setting.addr_other4 = { AF_INET, htons(setting.port_other) };
		inet_pton(AF_INET, setting.ip_Other, &(setting.addr_other4.sin_addr));
		setting.addr_self4 = { AF_INET, htons(setting.port_self) };

		setting.udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		int res = bind(setting.udp_socket, (SOCKADDR*)&(setting.addr_self4), sizeof(setting.addr_self4));
		if (res != 0)
			ErrorLog(pSetting, "fail to create socket", WSAGetLastError());
		InfoLog(pSetting, std::format("ipv4 connection, {} to {};port:{},{}", setting.ip_Self, setting.ip_Other, setting.port_self, setting.port_other));
		setting.is_ipv6 = false;
	}
	

	if (setting.is_blocking)
	{
		unsigned int timeout = 1;
		setsockopt(setting.udp_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)(&timeout), sizeof(timeout));
		setsockopt(setting.udp_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)(&timeout), sizeof(timeout));
	}else{
		u_long o = 1;
		ioctlsocket(setting.udp_socket, FIONBIO, &o);
	}
	//set init
	BYTE bytes[] = { 0x60,0xE8,0xD6,0x61,0xFC,0xFF,0x61,0x53,0x8B,0xDC,0xEB,0x02,0xEB,0xF2,0x90};
	*(DWORD*)(bytes+2)=(((DWORD)M_Init)-0x439E25-5);
	for (int i = 0; i < 15; i++)
		Address<BYTE>(0x00439E24 + i).SetValue(bytes[i]);

	QueryPerformanceFrequency(&(setting.timer_freq));
}

void Delay(Settings* ps, int millisec)
{
	LARGE_INTEGER start, end;
	QueryPerformanceCounter(&start);
	
	while (true)
	{
		QueryPerformanceCounter(&end);
		int ms=((end.QuadPart - start.QuadPart) * 1000 / (ps->timer_freq.QuadPart));
		if (ms >= millisec)
			return;
		if (millisec - ms >= 5)
			Sleep(3);
	}
}
int SendKey(Settings* ps, DataToSend data);
int RcvKey(Settings* ps, DataToSend* pData);
void __fastcall M_Init()
{
	auto ps= Address<Settings*>(p_cave).GetValue();
	ps->time_now = 0;
	ps->real_time_now = 0;
	*pSeedANM = ps->rnd_seed;
	*pSeedREP = ps->rnd_seed;
	ps->data_self.clear();
	ps->data_other.clear();
	ps->wait_time = 0;
	ps->err_occurred = 0;
	ps->data_self.emplace_back(0u,0,Check::DATA);
	ps->data_other.emplace_back(0u, 0, Check::DATA);
	ps->slp_time = 1;
	ps->nak_time = 0;
	//ps->real_control.push_back(0);

	// clear the UDP cache
	{
		DataToSend data;
		while (RcvKey(ps, &data) > 0) {
			if (data.check == Check::DATA && data.time<=ps->delay_frame+1+1) {
				while (ps->data_other.size() <= data.time)
					ps->data_other.emplace_back(ps->data_other[ps->data_other.size() - 1].control, ps->data_other.size(), Check::NAK);
				ps->data_other[data.time] = data;
			}
		}
	}
}

int SendKey(Settings* ps, DataToSend data)
{
	int l_nLen=0;
	if(ps->is_ipv6 && ps->addr_other.sin6_port!=0)
		l_nLen= sendto(ps->udp_socket, (const char*)&(data), sizeof(DataToSend), 0, (SOCKADDR*)&(ps->addr_other), sizeof(ps->addr_other));
	else if(!ps->is_ipv6 && ps->addr_other4.sin_port!=0)
		l_nLen= sendto(ps->udp_socket, (const char*)&(data), sizeof(DataToSend), 0, (SOCKADDR*)&(ps->addr_other4), sizeof(ps->addr_other4));
	if (l_nLen < 0)
		ErrorLog(ps, "send error",WSAGetLastError(), false);
	if (l_nLen > 0 && ps->is_debug)
	{
		const char* strs[] = { "NAK","ACK","DTA","ERR" };
		const char* ch;
		if ((int)data.check > 2 || (int)data.check < 0)
			ch = strs[3];
		else
			ch = strs[(int)(data.check)];
		InfoLog(ps, std::format("send,frame={},control={},check={}", data.time, data.control, ch));
	}
	
	return l_nLen;
}

int RcvKey(Settings* ps, DataToSend* pData)
{
	int l_nReadLen = 0;
	if(ps->port_other!=0)
		l_nReadLen = recvfrom(ps->udp_socket, (char*)(pData), sizeof(DataToSend), 0, nullptr,nullptr);
	else{
		int sz=0;
		if(ps->is_ipv6)
			sz = sizeof(ps->addr_other),l_nReadLen = recvfrom(ps->udp_socket, (char*)(pData), sizeof(DataToSend), 0, (SOCKADDR*)&(ps->addr_other), &sz);
		else
			sz = sizeof(ps->addr_other4), l_nReadLen = recvfrom(ps->udp_socket, (char*)(pData), sizeof(DataToSend), 0, (SOCKADDR*)&(ps->addr_other4),&sz);
	}

	if (l_nReadLen > 0 && ps->is_debug)
	{
		const char* strs[] = { "NAK","ACK","DTA","ERR"};
		const char* ch;
		if ((int)pData->check > 2 || (int)pData->check < 0)
			ch=strs[3];
		else
			ch = strs[(int)(pData->check)];
		InfoLog(ps, std::format("received,frame={},control={},check={}", pData->time, pData->control, ch));
	}
	if (l_nReadLen < 0)
	{
		int x = WSAGetLastError();
		ps->lasterr = x;
		if(x!=10035)
			ErrorLog(ps, "rcv error",x , false);
	}
	return l_nReadLen;
}

bool CheckNetRcv(Settings* ps)
{
	Delay(ps, 6);
	ps->wait_time++;
	if (ps->lasterr == 10054)
	{
		ErrorLog(ps, "connect terminated");
		ps->err_occurred = true;
	}
	if((ps->time_now>ps->delay_frame+10 && ps->wait_time >= ps->time_out) || (ps->wait_time>=ps->time_out*100))
	{
		if (ps->lasterr && ps->lasterr != 10060 && ps->lasterr != 10035)
			ErrorLog(ps, std::format("error occurred, last error:{}", ps->lasterr),0, !ps->err_occurred);
		else
			ErrorLog(ps, "time out", 0, !ps->err_occurred);
		ps->err_occurred = true;
	}
	return ps->err_occurred;
}

bool CheckNetSend(Settings* ps)
{
	if ((ps->is_ipv6 && ps->addr_other.sin6_port == 0) || (!ps->is_ipv6 && ps->addr_other4.sin_port == 0))
	{
			InfoLog(ps, "waiting client to enter");
			return 1;
	}
	Delay(ps, 6);
	return 0;
}

DWORD __fastcall GetControlKey(DWORD thiz, DWORD)
{
	Address<DWORD> addr(thiz+ 0xEA648);
	addr += 4;
	Player* ppl_cur=nullptr;
	if (addr.GetValue())
		ppl_cur = (Player*)addr.GetValue();
	Player* ppl_self = Address<Player*>(0x004D49C4).GetValue();
	Player * ppl_other = Address<Player*>(0x004D4A00).GetValue();
	auto ps = Address<Settings*>(p_cave).GetValue();
	if (!ps->is_main)
		std::swap(ppl_self, ppl_other);
	DWORD ctKey = *pControl_Key;
	DWORD resKey = ctKey;

	if (ps->err_occurred)
		return resKey;

	ps->wait_time = 0;
	//ps->real_time_now++;
	//if (((ps->real_time_now - 1) >> 1) % ps->jump_frame != 0)
	//{
	//	int t_delayed = ps->time_now - ps->delay_frame;
	//	if (t_delayed < 0)t_delayed = 0;
	//	if (ppl_cur == ppl_self) {
	//		resKey = ps->data_self[t_delayed].control;
	//	}
	//	else {
	//		resKey= ps->data_other[t_delayed].control;
	//	}
	//	return resKey;
	//}
	if (ps->updates == 0)
	{
		*pSeedANM = ps->time_now*10086+ps->rnd_seed;
		ps->time_now++;
		int t = ps->time_now;
		//ps->real_control.emplace_back(ctKey);//realcontrol[time_now]==ctKey
		ps->data_self.emplace_back(ctKey, t,Check::DATA);
		while(ps->data_other.size()<=t)
			ps->data_other.emplace_back(ps->data_other[ps->data_other.size() - 1].control, ps->data_other.size(), Check::NAK); 
		while (SendKey(ps, ps->data_self[t]) <= 0) { if(CheckNetSend(ps)) break; }
		//goto UPDATE;
		DataToSend data;
		std::vector<int> re_send;
		while (RcvKey(ps, &data) > 0){
			if (data.check == Check::NAK && data.time<=t) {
				re_send.push_back(data.time);
			}else if (data.check == Check::DATA) {
				while (ps->data_other.size()<= data.time)
					ps->data_other.emplace_back(ps->data_other[ps->data_other.size() - 1].control, ps->data_other.size(), Check::NAK); 
				
				ps->data_other[data.time] = data;
				for (int i = 1; i < ps->delay_frame; i++)
				{
					int t_test = data.time - i;
					if (t_test == 0)
						break;
					if (ps->data_other[t_test].check == Check::NAK){
						while (SendKey(ps, ps->data_other[t_test]) <= 0) { if (CheckNetSend(ps)) break; }
					}
				}
			}
		}
		re_send.erase(std::unique(re_send.begin(),re_send.end()),re_send.end());
		for (auto i : re_send)
			while (SendKey(ps, ps->data_self[i]) <= 0) if (CheckNetSend(ps)) break;
	}
UPDATE:
	ps->updates++;
	if (ps->updates == 2)
		ps->updates = 0;

	int t_delayed = ps->time_now - ps->delay_frame;
	if (t_delayed < 0)t_delayed = 0;

	if (ppl_cur == ppl_self){
		resKey = ps->data_self[t_delayed].control;
	}else{
		int i = 0;
		while (ps->data_other[t_delayed].check == Check::NAK){
			if (i == 0){
				i = 1;ps->nak_time++;
				if (ps->nak_time >= 10){
					Delay(ps, 32); ps->nak_time = 0;//sync with the host slower
					InfoLog(ps, "try to sync");
				}
			}
			DataToSend data;
			std::vector<int> re_send;
			while (RcvKey(ps, &data) > 0) {
				if (data.check == Check::NAK && data.time <= ps->time_now)
					re_send.push_back(data.time);
				else if (data.check == Check::DATA) {
					while (ps->data_other.size() <= data.time)
						ps->data_other.emplace_back(ps->data_other[ps->data_other.size() - 1].control, ps->data_other.size(), Check::NAK);
					if (ps->data_other[data.time].check == Check::ACK)
						ps->slp_time = std::min(ps->slp_time+1, 32);
					else
						ps->slp_time = std::max(8, ps->slp_time - 1);
					ps->data_other[data.time] = data;
					if (data.time == t_delayed)break;
				}
			}
			re_send.erase(std::unique(re_send.begin(), re_send.end()), re_send.end());
			for (auto i : re_send)
				while (SendKey(ps, ps->data_self[i]) <= 0) if (CheckNetSend(ps)) break;

			if (ps->err_occurred) { break; }
			if (ps->data_other[t_delayed].check != Check::NAK)
				break;
			while (SendKey(ps, ps->data_other[t_delayed]) <= 0) { if (CheckNetSend(ps)) break; }
			if (CheckNetRcv(ps)) break; 
			Delay(ps, ps->slp_time);
		}
		resKey = ps->data_other[t_delayed].control;
	}
	if (ps->is_save_control)
	{
		ps->control_file << std::format("{}:{},{};({},{})\n",ps->time_now,(int)(ppl_cur == ppl_self),resKey,*pSeedREP, *pSeedANM);
		ps->control_file.flush();
	}
	return resKey;
}