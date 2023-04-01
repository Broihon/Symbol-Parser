#pragma once

#include <Windows.h>

#include <algorithm>
#include <DbgHelp.h>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

#define SYMBOL_ERR_SUCCESS						0x00000000
#define SYMBOL_ERR_ALREADY_INITIALIZED			0x00000001
#define SYMBOL_ERR_CANT_OPEN_MODULE				0x00000002
#define SYMBOL_ERR_FILE_SIZE_IS_NULL			0x00000003
#define SYMBOL_ERR_CANT_ALLOC_MEMORY_NEW		0x00000004
#define SYMBOL_ERR_INVALID_FILE_ARCHITECTURE	0x00000005
#define SYMBOL_ERR_CANT_ALLOC_MEMORY			0x00000006
#define SYMBOL_ERR_NO_PDB_DEBUG_DATA			0x00000007
#define SYMBOL_ERR_PATH_DOESNT_EXIST			0x00000008
#define SYMBOL_ERR_CANT_CREATE_DIRECTORY		0x00000009
#define SYMBOL_ERR_CANT_CONVERT_PDB_GUID		0x0000000B
#define SYMBOL_ERR_GUID_TO_ANSI_FAILED			0x0000000C
#define SYMBOL_ERR_DOWNLOAD_FAILED				0x0000000D
#define SYMBOL_ERR_CANT_ACCESS_PDB_FILE			0x0000000E
#define SYMBOL_ERR_CANT_OPEN_PDB_FILE			0x0000000F
#define SYMBOL_ERR_IVNALID_SYMBOL_NAME			0x00000010
#define SYMBOL_ERR_CANT_OPEN_PROCESS			0x00000011
#define SYMBOL_ERR_SYM_INIT_FAIL				0x00000012
#define SYMBOL_ERR_SYM_LOAD_TABLE				0x00000013
#define SYMBOL_ERR_NOT_INITIALIZED				0x00000014
#define SYMBOL_ERR_SYMBOL_SEARCH_FAILED			0x00000015
#define SYMBOL_ERR_SYM_ENUM_SYMBOLS_FAILED		0x00000017
#define SYMBOL_ERR_STRING_CONVERSION_FAILED		0x00000018

struct SYM_INFO_COMPACT
{
	std::string szSymbol;
	DWORD RVA;
};

class SYMBOL_PARSER
{
	HANDLE			m_hProcess;
	HANDLE			m_hPdbFile;
	std::wstring	m_szPdbPath;
	DWORD			m_Filesize;
	DWORD64			m_SymbolTable;
	std::wstring	m_szModulePath;
	bool			m_Initialized;
	DWORD			m_LastWin32Error;

	bool VerifyExistingPdb(const GUID & guid);

	static constexpr auto sort_symbols_rva = [](const SYM_INFO_COMPACT & lhs, const SYM_INFO_COMPACT & rhs)
	{		
		return (lhs.RVA < rhs.RVA);
	};

	static constexpr auto sort_symbols_name = [](const SYM_INFO_COMPACT & lhs, const SYM_INFO_COMPACT & rhs)
	{
		return (_stricmp(lhs.szSymbol.c_str(), rhs.szSymbol.c_str()) < 0);
	};

	static constexpr auto sort_symbols_length = [](const SYM_INFO_COMPACT & lhs, const SYM_INFO_COMPACT & rhs)
	{
		return (lhs.szSymbol.length() < rhs.szSymbol.length());
	};

public:
	static const DWORD SymbolBase = 0x10000000;

	enum class SYMBOL_SORT
	{
		None,
		Rva,
		Name,
		Length
	};

	SYMBOL_PARSER();
	~SYMBOL_PARSER();

	DWORD Initialize(const std::wstring & szModulePath, const std::wstring & path, std::wstring * pdb_path_out, bool Redownload = false);
	
	DWORD GetSymbolAddress(std::string szSymbolName, DWORD & RvaOut);
	DWORD GetSymbolName(DWORD RvaIn, std::string & szSymbolNameOut);

	DWORD EnumSymbols(std::string szFilter, std::vector<SYM_INFO_COMPACT> & info);
	DWORD EnumSymbolsInRange(std::string szFilter, DWORD min_rva, DWORD max_rva, std::vector<SYM_INFO_COMPACT> & info);

	DWORD EnumSymbolsEx(std::string szFilter, std::vector<SYM_INFO_COMPACT> & info, SYMBOL_SORT sort = SYMBOL_SORT::None, bool ascending = true);
	DWORD EnumSymbolsInRangeEx(std::string szFilter, DWORD min_rva, DWORD max_rva, std::vector<SYM_INFO_COMPACT> & info, SYMBOL_SORT sort = SYMBOL_SORT::None, bool ascending = true);

	DWORD LastError();
};

//Thanks mambda
//https://bitbucket.org/mambda/pdb-parser/src/master/
struct PDBHeader7
{
	char signature[0x20];
	int page_size;
	int allocation_table_pointer;
	int file_page_count;
	int root_stream_size;
	int reserved;
	int root_stream_page_number_list_number;
};

struct RootStream7
{
	int num_streams;
	int stream_sizes[1]; //num_streams
};

struct GUID_StreamData
{
	int ver;
	int date;
	int age;
	GUID guid;
};

struct PdbInfo
{
	DWORD	Signature;
	GUID	Guid;
	DWORD	Age;
	char	PdbFileName[1];
};