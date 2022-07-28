#include "Symbol Parser.h"

#include <Windows.h>

auto constexpr TARGET_MODULE_PATH = L"C:\\Windows\\System32\\";
auto constexpr TARGET_MODULE_NAME = L"ntdll.dll";

//Symbol names to find
std::vector<std::string> SYMBOL_NAMES =
{
	"LdrpHeap",
	"LdrpLoadDll"
};

//Absolute address of symbol to identify
//GetSymbolName takes an RVA, however for easier copy&pasting ADDRESSES is filled with absolute addresses
//These example addresses won't work on your PC due to ASLR and potentially different ntdll.dll versions
//Instead paste your own addresses, alternatively feed your own RVAs into GetSymbolName
std::vector<ULONG_PTR> ADDRESSES =
{
#ifdef _WIN64
	0x7FFAC2741008,
	0x7FFAC2742FFC
#else
	0x775CDBA0,
	0x775CFF1C
#endif
};

//Relative virtual address of the symbol to identify
//As stated above, GetSymbolName takes an RVA which you can paste into this array
std::vector<DWORD> RVAS =
{
#ifdef _WIN64
	0x0001CC04,
	0x00174388
#else
	0x0004EF11,
	0x00126300
#endif
};

int main()
{
	std::wstring FullPath = TARGET_MODULE_PATH;
	FullPath += TARGET_MODULE_NAME;

	UINT_PTR mod_base = (UINT_PTR)LoadLibraryW(FullPath.c_str());

	wchar_t current_directory[MAX_PATH]{ 0 };
	if (!GetCurrentDirectoryW(sizeof(current_directory) / sizeof(wchar_t), current_directory))
	{
		printf("GetCurrentDirectoryW failed:\n\terror = 0x%08X\n", GetLastError());

		return 0;
	}
	else
	{
		printf("Current directory:\n\t%ls\n\n", current_directory);
	}

	printf("Module = %ls\n", FullPath.c_str());

	std::wstring Out;

	SYMBOL_PARSER symbol_parser;
	DWORD dwSymRet = symbol_parser.Initialize(FullPath, current_directory, &Out, false);
	if (dwSymRet != SYMBOL_ERR_SUCCESS)
	{
		printf("SYMBOL_PARSER::Initialize failed:\n");
		printf("\tSymbolErr    = 0x%08X\n", dwSymRet);
		printf("\tLastWin32Err = %08X\n", symbol_parser.LastError());

		if (symbol_parser.LastError() == INET_E_OBJECT_NOT_FOUND || symbol_parser.LastError() == INET_E_RESOURCE_NOT_FOUND)
		{
			printf("\tSymbol file not found on Microsoft Public Symbol Server\n");
		}

		return 0;
	}

	printf("Successfully initialized PDB:\n\tpath = %ls\n\n", Out.c_str());

	printf("Searching for symbol names:\n");

	for (const auto & i : SYMBOL_NAMES)
	{
		DWORD RvaOut = 0;

		dwSymRet = symbol_parser.GetSymbolAddress(i.c_str(), RvaOut);
		if (dwSymRet != SYMBOL_ERR_SUCCESS)
		{
			printf("   SYMBOL_PARSER::GetSymbolAddress failed:\n\tsymbol error = %08X\n\twin32 error  = %08X\n\tsymbol name  = %s\n", dwSymRet, symbol_parser.LastError(), i.c_str());
		}
		else
		{
			printf("   Found %s in %ls:\n\trva     = %08X\n\taddress = %ls+%08X (%p)\n", i.c_str(), TARGET_MODULE_NAME, RvaOut, TARGET_MODULE_NAME, RvaOut, (void *)(RvaOut + mod_base));
		}
	}

	printf("\nSearching for symbol names by absolute address:\n");
	for (auto i : ADDRESSES)
	{
		std::string name_out;

		DWORD RvaOut = (DWORD)(i - mod_base);
		dwSymRet = symbol_parser.GetSymbolName(RvaOut, name_out); //GetSymbolName takes an RVA, however for easier copy&pasting ADDRESSES is filled with absolute addresses

		if (dwSymRet != SYMBOL_ERR_SUCCESS)
		{
			printf("\tSYMBOL_PARSER::GetSymbolName failed:\n\t\tsymbol error = %08X\n\t\twin32 error  = %08X\n", dwSymRet, symbol_parser.LastError());
		}
		else
		{
			printf("\tFound symbolname of %ls+%08X:\n\t\t%s\n", TARGET_MODULE_NAME, RvaOut, name_out.c_str());
		}
	}

	printf("\nSearching for symbol names by relative virtual address:\n");
	for (auto i : RVAS)
	{
		std::string name_out;

		dwSymRet = symbol_parser.GetSymbolName(i, name_out);

		if (dwSymRet != SYMBOL_ERR_SUCCESS)
		{
			printf("\tSYMBOL_PARSER::GetSymbolName failed:\n\t\tsymbol error = %08X\n\t\twin32 error  = %08X\n", dwSymRet, symbol_parser.LastError());
		}
		else
		{
			printf("\tFound symbolname of %ls+%08X:\n\t\t%s\n", TARGET_MODULE_NAME, i, name_out.c_str());
		}
	}

	std::string Filter = "*Vectored*";
	printf("\nEnumerating all symbols containing the filter: %s\n", Filter.c_str());

	std::vector<SYM_INFO_COMPACT> info;
	symbol_parser.EnumSymbolsEx(Filter, info, SYMBOL_PARSER::SYMBOL_SORT::Name); //enumerate all symbols that contain the string "Vectored" and sort by name
	
	printf("Found %d result(s)\n", (int)info.size());
	for (const auto & i : info)
	{
		printf("%ls+%08X: %s\n", TARGET_MODULE_NAME, i.RVA, i.szSymbol.c_str());
	}

	return 0;
}