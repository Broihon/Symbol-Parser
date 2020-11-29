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
	dwSymRet = symbol_parser.GetSymbolAddress("LdrpLoadDll", RvaOut);
	if (dwSymRet != SYMBOL_ERR_SUCCESS)
	{
		printf("SYMBOL_PARSER::GetSymbolAddress failed with %08X\n", dwSymRet);

		return 0;
	}

	printf("Found LdrpLoadDll at ntdll.dll+%08X\n", RvaOut);

	std::string name_out;
	dwSymRet = symbol_parser.GetSymbolName(RvaOut, name_out);
	if (dwSymRet != SYMBOL_ERR_SUCCESS)
	{
		printf("SYMBOL_PARSER::GetSymbolName failed with %08X\n", dwSymRet);

		return 0;
	}

	printf("Symbolname of ntdll.dll+%08X is %s\n", RvaOut, name_out.c_str());

	std::vector<SYM_INFO_COMPACT> info;
	symbol_parser.EnumSymbols("LdrpLoad*", info);

	for (auto & i : info)
	{
		printf("ntdll.dl+%08X: %s\n", i.RVA, i.szSymbol.c_str());
	}

	return 0;
}