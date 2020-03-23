#include "Symbol Parser.h"

int main()
{
	char current_directory[MAX_PATH]{ 0 };
	if (!GetCurrentDirectoryA(sizeof(current_directory), current_directory))
	{
		printf("GetCurrentDirectoryA failed: 0x%08X\n", GetLastError());

		return 0;
	}

	std::string DllFile("C:\\Windows\\SysWOW64\\ntdll.dll");
	std::string Out;

	SYMBOL_PARSER symbol_parser;
	DWORD dwSymRet = symbol_parser.Initialize(DllFile, current_directory, Out, false);
	if (dwSymRet != SYMBOL_ERR_SUCCESS)
	{
		printf("SYMBOL_PARSER::Initialize failed: 0x%08X\n", dwSymRet);

		return 0;
	}

	printf("Successfully downloaded PDB:\n%s\n\n", Out.c_str());

	DWORD out = 0;
	dwSymRet = symbol_parser.GetSymbolAddress("LdrpLoadDll", out);
	if (dwSymRet != SYMBOL_ERR_SUCCESS)
	{
		printf("SYMBOL_PARSER::GetSymbolAddress failed with %08X\n", dwSymRet);

		return 0;
	}

	printf("Located LdrpLoadDll at ntdll.dll+%08X\n", out);

	return 0;
}