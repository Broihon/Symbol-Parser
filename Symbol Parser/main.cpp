#include "Symbol Parser.h"

#include <windows.h>

typedef struct _UNICODE_STRING
{
	WORD		Length;
	WORD		MaxLength;
	wchar_t * szBuffer;
} UNICODE_STRING, * PUNICODE_STRING;

using f_LdrpApplyFileNameRedirection = NTSTATUS(__fastcall *)
(
	void *,
	UNICODE_STRING * DllName,
	void *,
	UNICODE_STRING * OutputDllName,
	BOOLEAN * sxs
);

int main()
{
	char current_directory[MAX_PATH]{ 0 };
	if (!GetCurrentDirectoryA(sizeof(current_directory), current_directory))
	{
		printf("GetCurrentDirectoryA failed: 0x%08X\n", GetLastError());

		return 0;
	}
	else
	{
		printf("Current directory:\n%s\n\n", current_directory);
	}

	std::string DllFile("C:\\Windows\\System32\\ntdll.dll");
	std::string Out;

	SYMBOL_PARSER symbol_parser;
	DWORD dwSymRet = symbol_parser.Initialize(DllFile, current_directory, &Out, false);
	if (dwSymRet != SYMBOL_ERR_SUCCESS)
	{
		printf("SYMBOL_PARSER::Initialize failed: 0x%08X\n", dwSymRet);

		return 0;
	}

	printf("Successfully initialized PDB:\n%s\n\n", Out.c_str());

	DWORD RvaOut = 0;
	dwSymRet = symbol_parser.GetSymbolAddress("RtlpWaitOnAddressWithTimeout", RvaOut);
	if (dwSymRet != SYMBOL_ERR_SUCCESS)
	{
		printf("SYMBOL_PARSER::GetSymbolAddress failed with %08X\n", dwSymRet);

		return 0;
	}

	printf("%p\n", (void*)((UINT_PTR)GetModuleHandle(TEXT("ntdll.dll")) + RvaOut));

	char xd[MAX_PATH]{ 0 };
	symbol_parser.GetSymbolName(0x7598, xd);
	printf("%s\n", xd);

	return 0;
}