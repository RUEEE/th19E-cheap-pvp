#include "pch.h"
#include "Address.h"
#include "AI_hook.h"
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

const DWORD p_codecave = 0x00420754;


#include<winsock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
enum Check
{
	ACK = 1, NAK = 0, NOT = 2
};

struct DataToSend
{
	DWORD control = 0;
	int time = 0;
	Check check = Check::NOT;
};

struct NetSetting
{
	sockaddr_in6 this_addr;
	sockaddr_in6 to_addr;
	char addr_host_this[128] = { 0 };
	char addr_host_to[128] = { 0 };
	int port_host_this = 0;
	int port_host_to = 0;
	int send_frame = 1;
	int time_out = 60;
	bool is_main = false;
	SOCKET udp_socket;
	int time_now;
	DataToSend last_data_this;
	DataToSend last_data_to;
	DataToSend cur_data_this;
	DataToSend cur_data_to;
	int updates=0;
	int lasterr = 0;
	DWORD rnd_seed;
};



void __fastcall M_Init();

bool Init()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		MessageBoxW(NULL, L"fail to init wsa", L"Error", MB_OK);
		return false;
	}
	NetSetting* pSetting = new NetSetting();
	NetSetting& setting=*pSetting;
	Address<NetSetting*> ns(p_codecave);//a code cave
	ns.SetValue(pSetting);
	// read settings
	GetPrivateProfileStringA("network","IPV6_ADDR_HOST_THIS","::1", setting.addr_host_this,sizeof(setting.addr_host_this), ".\\net.ini");
	GetPrivateProfileStringA("network", "IPV6_ADDR_HOST_TO", "::1", setting.addr_host_to, sizeof(setting.addr_host_to), ".\\net.ini");
	char buf[128] = { 0 };
	GetPrivateProfileStringA("network", "IPV6_PORT_HOST_THIS", "0", buf, sizeof(buf), ".\\net.ini");
	setting.port_host_this =atoi(buf);
	GetPrivateProfileStringA("network", "IPV6_PORT_HOST_TO", "0", buf, sizeof(buf), ".\\net.ini");
	setting.port_host_to = atoi(buf);
	GetPrivateProfileStringA("network", "SEND_FRAME", "1", buf, sizeof(buf), ".\\net.ini");
	setting.send_frame = atoi(buf);
	GetPrivateProfileStringA("network", "TIME_OUT", "60", buf, sizeof(buf), ".\\net.ini");
	setting.time_out = atoi(buf);
	GetPrivateProfileStringA("network", "IS_MAIN", "0", buf, sizeof(buf), ".\\net.ini");
	setting.is_main = (atoi(buf))==1;
	GetPrivateProfileStringA("network", "RNG", "0", buf, sizeof(buf), ".\\net.ini");
	setting.rnd_seed = atoi(buf);
	if (setting.port_host_this == 0 || setting.port_host_to == 0)
	{
		MessageBoxW(NULL, L"fail to read ports in net.ini", L"Error", MB_OK);
		return false;
	}
	if (setting.send_frame <= 0)
		setting.send_frame = 1;
	
	// udp
	setting.to_addr = { AF_INET6, htons(setting.port_host_to) };
	inet_pton(AF_INET6, setting.addr_host_to, &(setting.to_addr.sin6_addr));
	setting.this_addr = { AF_INET6, htons(setting.port_host_this) };

	setting.udp_socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	int res=bind(setting.udp_socket, (SOCKADDR*)&(setting.this_addr), sizeof(setting.this_addr));
	if (res != 0)
		MessageBoxA(NULL, "fail to create socket", "Error", MB_OK);

	//set init
	BYTE bytes[] = { 0x60,0xE8,0xD6,0x61,0xFC,0xFF,0x61,0x53,0x8B,0xDC,0xEB,0x02,0xEB,0xF2,0x90};
	*(DWORD*)(bytes+2)=(((DWORD)M_Init)-0x439E25-5);
	for (int i = 0; i < 15; i++)
		Address<BYTE>(0x00439E24 + i).SetValue(bytes[i]);
}

void __fastcall M_Init()
{
	auto ps= Address<NetSetting*>(p_codecave).GetValue();
	ps->time_now = 1;
	Address<DWORD> rnd(0x004D4980);
	rnd.SetValue(ps->rnd_seed);
}

bool SendKey(NetSetting* ps)
{
	int l_nLen = sendto(ps->udp_socket, (const char*)&(ps->cur_data_this), sizeof(DataToSend), 0, (SOCKADDR*)&(ps->to_addr), sizeof(ps->to_addr));
	if (l_nLen < 0)
	{
		ps->lasterr = WSAGetLastError();
	}
	return l_nLen > 0;
}

bool RcvKey(NetSetting* ps)
{
	int l_nReadLen = recvfrom(ps->udp_socket, (char*)&(ps->cur_data_to), sizeof(DataToSend), 0, nullptr,nullptr);
	if (l_nReadLen < 0)
	{
		ps->lasterr= WSAGetLastError();
	}
	return l_nReadLen > 0;
}

DWORD __fastcall GetControlKey(DWORD thiz, DWORD)
{
	Address<DWORD> addr(thiz+ 0xEA648);
	addr += 4;
	Player* ppl=nullptr;
	if (addr.GetValue())
		ppl = (Player*)addr.GetValue();
	Player* pl_left = Address<Player*>(0x004D49C4).GetValue();
	Player * pl_right = Address<Player*>(0x004D4A00).GetValue();

	Address<NetSetting*> ns(p_codecave);//a code cave
	auto ps = ns.GetValue();
	DWORD ctKey = *pControl_Key;
	DWORD resKey=ctKey;
	if (ps->time_now % ps->send_frame == 0)
	{
		if (ps->updates==0){
			ps->last_data_this = ps->cur_data_this;
			ps->last_data_to = ps->cur_data_to;
			ps->cur_data_this.control = ctKey;
			ps->cur_data_this.time = ps->time_now;
			ps->cur_data_this.check = Check::NOT;
			while (!SendKey(ps)) { Sleep(1);}
		}
		if (!ps->is_main)
			std::swap(pl_left, pl_right);
		if (ppl == pl_left){
			ps->updates++;
			resKey = ps->cur_data_this.control;
		}else {
			ps->updates++;
			int i = 0;
			while (true)
			{
				while (!RcvKey(ps))
				{
					Sleep(1);
					i++;
					if (i >= ps->time_out) {
						ps->cur_data_this.check = Check::NAK;
						while (!SendKey(ps)) { Sleep(1); }
						ps->cur_data_this.check = Check::NOT;
						Sleep(1);
					}
				}
				if (ps->cur_data_to.check == Check::NAK)
				{
					if (ps->cur_data_to.time == ps->cur_data_this.time)
					{
						while (!SendKey(ps)) { Sleep(1); }continue;
					}else {
						auto tmp = ps->cur_data_this; 
						ps->cur_data_this = ps->last_data_this;
						while (!SendKey(ps)) { Sleep(1); }continue;
						ps->cur_data_this = tmp;
					}
				}else if (ps->cur_data_to.check == Check::NOT)
					break;
			}
			resKey = ps->cur_data_to.control;
		}
		if (ps->updates >= 2)
			ps->updates = 0, ps->time_now++;
	}else{
		if (ps->updates==0){
			ps->last_data_this = ps->cur_data_this;
			ps->last_data_to = ps->cur_data_to;
			ps->cur_data_this.time = ps->time_now;
			ps->cur_data_to.time = ps->time_now;
		}
		if (ps->is_main)
			resKey = (ppl == pl_left) ? ps->last_data_this.control : ps->last_data_to.control, ps->updates++;
		else
			resKey = (ppl == pl_left) ? ps->last_data_to.control : ps->last_data_this.control, ps->updates++;
		if (ps->updates >= 2)
			ps->updates = 0, ps->time_now++;
	}

	return resKey;
}