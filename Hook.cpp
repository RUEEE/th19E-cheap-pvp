#include "pch.h"
#include "Address.h"
#include "Hook.h"
#include "AI_hook.h"
void HookAll()
{
	Address<DWORD>(0x004BBE38).SetValue((DWORD)GetControlKey);
	Address<DWORD>(0x004BBE48).SetValue((DWORD)GetControlKey);
	Address<DWORD>(0x004BBE58).SetValue((DWORD)GetControlKey);
}