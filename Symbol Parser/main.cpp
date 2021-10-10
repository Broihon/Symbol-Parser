#include "Symbol Parser.h"

#include <Windows.h>

auto constexpr TARGET_MODULE_PATH = "C:\\Windows\\System32\\";
auto constexpr TARGET_MODULE_NAME = "ntdll.dll";

//Symbol names to find
std::vector<std::string> SYMBOL_NAMES =
{
	"LdrpVectorHandlerList"
};

//Absolute address of symbol to identify
//GetSymbolName takes an RVA, however for easier copy&pasting ADDRESSES is filled with absolute addresses
//These example addresses won't work on your PC due to ASLR and potentially different ntdll.dll versions
std::vector<ULONG_PTR> ADDRESSES =
{
#ifdef _WIN64
	0x7FFA63771010,
	0x7FFE54E16ED0,
#else
	0xDEADBEEF,
	0x73C0AD4C,
#endif
};

int main()
{
	UINT_PTR mod_base = (UINT_PTR)LoadLibraryA(TARGET_MODULE_NAME);

	char current_directory[MAX_PATH]{ 0 };
	if (!GetCurrentDirectoryA(sizeof(current_directory), current_directory))
	{
		printf("GetCurrentDirectoryA failed:\n\terror = 0x%08X\n", GetLastError());

		return 0;
	}
	else
	{
		printf("Current directory:\n\t%s\n\n", current_directory);
	}

	std::string DllFile(TARGET_MODULE_PATH);
	DllFile += TARGET_MODULE_NAME;

	std::string Out;

	SYMBOL_PARSER symbol_parser;
	DWORD dwSymRet = symbol_parser.Initialize(DllFile, current_directory, &Out, false);
	if (dwSymRet != SYMBOL_ERR_SUCCESS)
	{
		printf("SYMBOL_PARSER::Initialize failed:\n\terror = 0x%08X\n", dwSymRet);

		return 0;
	}

	printf("Successfully initialized PDB:\n\tpath = %s\n", Out.c_str());

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
			printf("   Found %s in %s:\n\trva     = %08X\n\taddress = %s+%08X (%p)\n", i.c_str(), TARGET_MODULE_NAME, RvaOut, TARGET_MODULE_NAME, RvaOut, (void *)(RvaOut + mod_base));
		}
	}

	printf("\n");

	for (auto i : ADDRESSES)
	{
		std::string name_out;

		DWORD RvaOut = (DWORD)(i - mod_base);
		dwSymRet = symbol_parser.GetSymbolName(RvaOut, name_out); //GetSymbolName takes an RVA, however for easier copy&pasting ADDRESSES is filled with absolute addresses

		if (dwSymRet != SYMBOL_ERR_SUCCESS)
		{
			printf("SYMBOL_PARSER::GetSymbolName failed:\n\tsymbol error = %08X\n\twin32 error  = %08X\n", dwSymRet, symbol_parser.LastError());
		}
		else
		{
			printf("Found symbolname of %s+%08X:\n\t%s\n\n", TARGET_MODULE_NAME, RvaOut, name_out.c_str());
		}
	}

	printf("\n");	
	
	std::vector<SYM_INFO_COMPACT> info;
	symbol_parser.EnumSymbolsEx("*VectoredHandler*", info, SYMBOL_PARSER::SYMBOL_SORT::Name); //enumerate all symbols that contain the string "VectoredHandler" and sort by name
	for (const auto & i : info)
	{
		printf("%s+%08X: %s\n", TARGET_MODULE_NAME, i.RVA, i.szSymbol.c_str());
	}
	
	printf("count = %d\n", (int)info.size());

	return 0;
}