#include "pch.h"
#include "Address.h"
#include "AI_hook.h"
#include <timeapi.h>
#include <vector>
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


#include<winsock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"winmm.lib")
enum Check
{
	ACK = 1, NAK = 0, DATA = 2
};

struct DataToSend
{
	DWORD control = 0;
	int time = 0;
	Check check = Check::DATA;
	DataToSend(DWORD c=0, int t=0, Check ck = Check::DATA) :control(c), time(t), check(ck) {};
};

struct Settings
{
	sockaddr_in6 addr_self;
	sockaddr_in6 addr_other;
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
};

void __fastcall M_Init();

bool Init()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		MessageBoxW(NULL, L"fail to init wsa", L"Error", MB_OK);
		return false;
	}
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
	//GetPrivateProfileStringA("network", "JUMP_FRAME", "2", buf, sizeof(buf), ".\\net.ini");
	//setting.jump_frame = atoi(buf);
	GetPrivateProfileStringA("network", "RNG", "0", buf, sizeof(buf), ".\\net.ini");
	setting.rnd_seed = atoi(buf);
	if (setting.port_self == 0 || setting.port_other == 0)
	{
		MessageBoxW(NULL, L"fail to read ports in net.ini", L"Error", MB_OK);
		return false;
	}
	if (setting.delay_frame <= 0)
		setting.delay_frame = 1;
	
	// udp
	setting.addr_other = { AF_INET6, htons(setting.port_other) };
	inet_pton(AF_INET6, setting.ip_Other, &(setting.addr_other.sin6_addr));
	setting.addr_self = { AF_INET6, htons(setting.port_self) };

	setting.udp_socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	int res=bind(setting.udp_socket, (SOCKADDR*)&(setting.addr_self), sizeof(setting.addr_self));
	if (res != 0)
		MessageBoxA(NULL, "fail to create socket", "Error", MB_OK);
	//unsigned int timeout = 1;
	//setsockopt(setting.udp_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)(&timeout), sizeof(timeout));
	u_long o = 1;
	ioctlsocket(setting.udp_socket, FIONBIO, &o);
	//setsockopt(setting.udp_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)(&timeout), sizeof(timeout));

	//set init
	BYTE bytes[] = { 0x60,0xE8,0xD6,0x61,0xFC,0xFF,0x61,0x53,0x8B,0xDC,0xEB,0x02,0xEB,0xF2,0x90};
	*(DWORD*)(bytes+2)=(((DWORD)M_Init)-0x439E25-5);
	for (int i = 0; i < 15; i++)
		Address<BYTE>(0x00439E24 + i).SetValue(bytes[i]);

	timeBeginPeriod(1);
}

void __fastcall M_Init()
{
	auto ps= Address<Settings*>(p_cave).GetValue();
	ps->time_now = 0;
	ps->real_time_now = 0;
	Address<DWORD> rnd(0x004D4980);
	rnd.SetValue(ps->rnd_seed);
	ps->data_self.clear();
	ps->data_other.clear();
	ps->wait_time = 0;
	ps->err_occurred = 0;
	ps->data_self.emplace_back(0u,0,Check::DATA);
	ps->data_other.emplace_back(0u, 0, Check::DATA);
	//ps->real_control.push_back(0);
}

int SendKey(Settings* ps, DataToSend data)
{
	int l_nLen = sendto(ps->udp_socket, (const char*)&(data), sizeof(DataToSend), 0, (SOCKADDR*)&(ps->addr_other), sizeof(ps->addr_other));
	if (l_nLen < 0)
		ps->lasterr = WSAGetLastError();
	return l_nLen;
}

int RcvKey(Settings* ps, DataToSend* pData)
{
	int l_nReadLen = recvfrom(ps->udp_socket, (char*)(pData), sizeof(DataToSend), 0, nullptr,nullptr);
	if (l_nReadLen < 0)
		ps->lasterr= WSAGetLastError();
	return l_nReadLen;
}

bool CheckNetRcv(Settings* ps)
{
	Sleep(1);
	ps->wait_time++;
	char warning[100];
	if (ps->wait_time >= ps->time_out)
	{
		if (ps->lasterr && ps->lasterr!=10060)
			sprintf_s(warning, sizeof(warning), "error might have happened, code: %d", ps->lasterr);
		else
			sprintf_s(warning, sizeof(warning), "time out");
		if (!ps->err_occurred)
			MessageBoxA(NULL, warning, "Error", MB_OK);
		ps->err_occurred = true;
	}
	return ps->err_occurred;
}

bool CheckNetSend(Settings* ps)
{
	Sleep(1);
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
	if (ps->is_main)
		std::swap(ppl_self, ppl_other);
	DWORD ctKey = *pControl_Key;
	DWORD resKey=ctKey;

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
		ps->time_now++;
		int t = ps->time_now;
		//ps->real_control.emplace_back(ctKey);//realcontrol[time_now]==ctKey
		ps->data_self.emplace_back(ctKey, t,Check::DATA);
		while(ps->data_other.size()<=t)
			ps->data_other.emplace_back(ps->data_other[ps->data_other.size() - 1].control, ps->data_other.size(), Check::NAK); 
		while (SendKey(ps, ps->data_self[t]) <= 0) { if(CheckNetSend(ps)) break; }
		DataToSend data;
		while (RcvKey(ps, &data) > 0){
			if (data.check == Check::NAK && data.time<=t) {
				while (SendKey(ps, ps->data_self[data.time]) <= 0) { if (CheckNetSend(ps)) break; }
			}
			else if (data.check == Check::DATA) {
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
	}
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
			if (ps->data_other.size()==t_delayed+1 && i==0)
				Sleep(2),i=1;
			DataToSend data;
			while (RcvKey(ps, &data) > 0) {
				if (data.check == Check::NAK && data.time <= ps->time_now)
					while (SendKey(ps, ps->data_self[data.time]) <= 0) { if (CheckNetSend(ps)) break; }
				else if (data.check == Check::DATA) {
					while (ps->data_other.size() <= data.time)
						ps->data_other.emplace_back(ps->data_other[ps->data_other.size() - 1].control, ps->data_other.size(), Check::NAK);
					ps->data_other[data.time] = data;
					if (data.time == t_delayed)break;
				}
			}
			if (ps->err_occurred) { break; }
			if (ps->data_other[t_delayed].check != Check::NAK)
				break;
			while (SendKey(ps, ps->data_other[t_delayed]) <= 0) { if (CheckNetSend(ps)) break; }
			if (CheckNetRcv(ps)) break; 
			Sleep(2);
		}
		resKey = ps->data_other[t_delayed].control;
	}
	return resKey;
}