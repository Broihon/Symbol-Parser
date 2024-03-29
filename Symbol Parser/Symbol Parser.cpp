#include "Symbol Parser.h"

#pragma comment(lib, "DbgHelp.lib")
#pragma comment(lib, "Urlmon.lib")

#define ReCa reinterpret_cast

SYMBOL_PARSER::SYMBOL_PARSER()
{
	m_Initialized		= false;
	m_SymbolTable		= 0;
	m_Filesize			= 0;
	m_hPdbFile			= nullptr;
	m_hProcess			= nullptr;
	m_LastWin32Error	= 0;
}

SYMBOL_PARSER::~SYMBOL_PARSER()
{
	if (m_Initialized)
	{
		SymUnloadModule64(m_hProcess, m_SymbolTable);

		SymCleanup(m_hProcess);

		CloseHandle(m_hProcess);
		CloseHandle(m_hPdbFile);

		m_Initialized = false;
	}
}

bool SYMBOL_PARSER::VerifyExistingPdb(const GUID & guid)
{
	std::ifstream f(m_szPdbPath.c_str(), std::ios::binary | std::ios::ate);
	if (f.bad())
	{
		return false;
	}

	size_t size_on_disk = static_cast<size_t>(f.tellg());
	if (!size_on_disk)
	{
		f.close();

		return false;
	}

	char * pdb_raw = new char[size_on_disk];
	if (!pdb_raw)
	{
		f.close();

		return false;
	}

	f.seekg(std::ios::beg);
	f.read(pdb_raw, size_on_disk);
	f.close();

	if (size_on_disk < sizeof(PDBHeader7))
	{
		delete[] pdb_raw;

		return false;
	}

	auto * pPDBHeader = ReCa<PDBHeader7 *>(pdb_raw);

	if (memcmp(pPDBHeader->signature, "Microsoft C/C++ MSF 7.00\r\n\x1A""DS\0\0\0", sizeof(PDBHeader7::signature)))
	{
		delete[] pdb_raw;

		return false;
	}

	if (size_on_disk < (size_t)pPDBHeader->page_size * pPDBHeader->file_page_count)
	{
		delete[] pdb_raw;

		return false;
	}

	int		* pRootPageNumber	= ReCa<int *>(pdb_raw + (size_t)pPDBHeader->root_stream_page_number_list_number * pPDBHeader->page_size);
	auto	* pRootStream		= ReCa<RootStream7 *>(pdb_raw + (size_t)(*pRootPageNumber) * pPDBHeader->page_size);

	std::map<int, std::vector<int>> streams;
	int current_page_number = 0;

	for (int i = 0; i != pRootStream->num_streams; ++i)
	{
		int current_size = pRootStream->stream_sizes[i] == 0xFFFFFFFF ? 0 : pRootStream->stream_sizes[i];

		int current_page_count = current_size / pPDBHeader->page_size;
		if (current_size % pPDBHeader->page_size)
		{
			++current_page_count;
		}

		std::vector<int> numbers;

		for (int j = 0; j != current_page_count; ++j, ++current_page_number)
		{
			numbers.push_back(pRootStream->stream_sizes[pRootStream->num_streams + current_page_number]);
		}

		streams.insert({ i, numbers });
	}

	auto pdb_info_page_index = streams.at(1).at(0);

	auto * stram_data = ReCa<GUID_StreamData *>(pdb_raw + (size_t)(pdb_info_page_index)*pPDBHeader->page_size);

	int guid_eq = memcmp(&stram_data->guid, &guid, sizeof(GUID));

	delete[] pdb_raw;

	return (guid_eq == 0);
}

DWORD SYMBOL_PARSER::Initialize(const std::wstring & szModulePath, const std::wstring & path, std::wstring * pdb_path_out, bool Redownload)
{
	if (m_Initialized)
	{
		return SYMBOL_ERR_ALREADY_INITIALIZED;
	}

	std::ifstream File(szModulePath, std::ios::binary | std::ios::ate);
	if (!File.good())
	{
		return SYMBOL_ERR_CANT_OPEN_MODULE;
	}

	auto FileSize = File.tellg();
	if (!FileSize)
	{
		return SYMBOL_ERR_FILE_SIZE_IS_NULL;
	}

	BYTE * pRawData = new(std::nothrow) BYTE[static_cast<size_t>(FileSize)];
	if (!pRawData)
	{
		delete[] pRawData;

		File.close();

		return SYMBOL_ERR_CANT_ALLOC_MEMORY_NEW;
	}

	File.seekg(0, std::ios::beg);
	File.read(ReCa<char *>(pRawData), FileSize);
	File.close();

	IMAGE_DOS_HEADER	* pDos	= ReCa<IMAGE_DOS_HEADER *>(pRawData);
	IMAGE_NT_HEADERS	* pNT	= ReCa<IMAGE_NT_HEADERS *>(pRawData + pDos->e_lfanew);
	IMAGE_FILE_HEADER	* pFile	= &pNT->FileHeader;

	IMAGE_OPTIONAL_HEADER64 * pOpt64 = nullptr;
	IMAGE_OPTIONAL_HEADER32 * pOpt32 = nullptr;

	bool x86 = false;

	if (pFile->Machine == IMAGE_FILE_MACHINE_AMD64)
	{
		pOpt64 = ReCa<IMAGE_OPTIONAL_HEADER64 *>(&pNT->OptionalHeader);
	}
	else if (pFile->Machine == IMAGE_FILE_MACHINE_I386)
	{
		pOpt32 = ReCa<IMAGE_OPTIONAL_HEADER32 *>(&pNT->OptionalHeader);
		x86 = true;
	}
	else
	{
		delete[] pRawData;

		return SYMBOL_ERR_INVALID_FILE_ARCHITECTURE;
	}

	DWORD ImageSize = x86 ? pOpt32->SizeOfImage : pOpt64->SizeOfImage;
	BYTE * pLocalImageBase = ReCa<BYTE *>(VirtualAlloc(nullptr, ImageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
	if (!pLocalImageBase)
	{
		m_LastWin32Error = GetLastError();

		delete[] pRawData;

		return SYMBOL_ERR_CANT_ALLOC_MEMORY;
	}

	memcpy(pLocalImageBase, pRawData, x86 ? pOpt32->SizeOfHeaders : pOpt64->SizeOfHeaders);

	auto * pCurrentSectionHeader = IMAGE_FIRST_SECTION(pNT);
	for (UINT i = 0; i != pFile->NumberOfSections; ++i, ++pCurrentSectionHeader)
	{
		if (pCurrentSectionHeader->SizeOfRawData)
		{
			memcpy(pLocalImageBase + pCurrentSectionHeader->VirtualAddress, pRawData + pCurrentSectionHeader->PointerToRawData, pCurrentSectionHeader->SizeOfRawData);
		}
	}

	IMAGE_DATA_DIRECTORY * pDataDir = nullptr;
	if (x86)
	{
		pDataDir = &pOpt32->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
	}
	else
	{
		pDataDir = &pOpt64->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
	}

	IMAGE_DEBUG_DIRECTORY * pDebugDir = ReCa<IMAGE_DEBUG_DIRECTORY *>(pLocalImageBase + pDataDir->VirtualAddress);

	if (!pDataDir->Size || IMAGE_DEBUG_TYPE_CODEVIEW != pDebugDir->Type)
	{
		VirtualFree(pLocalImageBase, 0, MEM_RELEASE);

		delete[] pRawData;

		return SYMBOL_ERR_NO_PDB_DEBUG_DATA;
	}

	PdbInfo * pdb_info = ReCa<PdbInfo *>(pLocalImageBase + pDebugDir->AddressOfRawData);
	if (pdb_info->Signature != 0x53445352)
	{
		VirtualFree(pLocalImageBase, 0, MEM_RELEASE);

		delete[] pRawData;

		return SYMBOL_ERR_NO_PDB_DEBUG_DATA;
	}

	m_szPdbPath = path;

	if (m_szPdbPath[m_szPdbPath.length() - 1] != '\\')
	{
		m_szPdbPath += '\\';
	}

	if (!CreateDirectoryW(m_szPdbPath.c_str(), nullptr))
	{
		if (GetLastError() != ERROR_ALREADY_EXISTS)
		{
			m_LastWin32Error = GetLastError();

			return SYMBOL_ERR_PATH_DOESNT_EXIST;
		}
	}

	m_szPdbPath += x86 ? L"x86\\" : L"x64\\";

	if (!CreateDirectoryW(m_szPdbPath.c_str(), nullptr))
	{
		if (GetLastError() != ERROR_ALREADY_EXISTS)
		{
			m_LastWin32Error = GetLastError();

			return SYMBOL_ERR_CANT_CREATE_DIRECTORY;
		}
	}

	size_t len = lstrlenA(pdb_info->PdbFileName);
	wchar_t * PdbFileNameW = new(std::nothrow) wchar_t[len + 1]();
	if (!PdbFileNameW)
	{
		return SYMBOL_ERR_CANT_ALLOC_MEMORY_NEW;
	}

	size_t SizeOut = 0;
	auto conversion_ret = mbstowcs_s(&SizeOut, PdbFileNameW, len + 1, pdb_info->PdbFileName, len);
	if (conversion_ret != 0 || !SizeOut)
	{
		m_LastWin32Error = conversion_ret;

		delete[] PdbFileNameW;

		return SYMBOL_ERR_STRING_CONVERSION_FAILED;
	}

	m_szPdbPath += PdbFileNameW;

	delete[] PdbFileNameW;

	DWORD Filesize = 0;
	WIN32_FILE_ATTRIBUTE_DATA file_attr_data{ 0 };
	if (GetFileAttributesExW(m_szPdbPath.c_str(), GetFileExInfoStandard, &file_attr_data))
	{
		Filesize = file_attr_data.nFileSizeLow;

		if (!Redownload && !VerifyExistingPdb(pdb_info->Guid))
		{
			Redownload = true;
		}

		if (Redownload)
		{
			DeleteFileW(m_szPdbPath.c_str());
		}
	}
	else
	{
		Redownload = true;
	}

	if (Redownload)
	{
		wchar_t w_GUID[100]{ 0 };
		if (!StringFromGUID2(pdb_info->Guid, w_GUID, 100))
		{
			VirtualFree(pLocalImageBase, 0, MEM_RELEASE);

			delete[] pRawData;

			return SYMBOL_ERR_CANT_CONVERT_PDB_GUID;
		}

		printf("GUID = %ls\n", w_GUID);

		char a_GUID[100]{ 0 };
		size_t l_GUID = 0;
		conversion_ret = wcstombs_s(&l_GUID, a_GUID, w_GUID, sizeof(a_GUID));
		if (conversion_ret != 0 || !l_GUID)
		{
			m_LastWin32Error = conversion_ret;

			VirtualFree(pLocalImageBase, 0, MEM_RELEASE);

			delete[] pRawData;

			return SYMBOL_ERR_GUID_TO_ANSI_FAILED;
		}

		std::string guid_filtered;
		for (UINT i = 0; i != l_GUID; ++i)
		{
			if ((a_GUID[i] >= '0' && a_GUID[i] <= '9') || (a_GUID[i] >= 'A' && a_GUID[i] <= 'F') || (a_GUID[i] >= 'a' && a_GUID[i] <= 'f'))
			{
				guid_filtered += a_GUID[i];
			}
		}

		char age[3]{ 0 };
		_itoa_s(pdb_info->Age, age, 10);

		std::string url = "https://msdl.microsoft.com/download/symbols/";
		url += pdb_info->PdbFileName;
		url += '/';
		url += guid_filtered;
		url += age;
		url += '/';
		url += pdb_info->PdbFileName;

		len = url.length();
		wchar_t * UrlW = new(std::nothrow) wchar_t[len + 1]();
		if (!UrlW)
		{
			return SYMBOL_ERR_CANT_ALLOC_MEMORY_NEW;
		}

		SizeOut = 0;
		conversion_ret = mbstowcs_s(&SizeOut, UrlW, len + 1, url.c_str(), len);
		if (conversion_ret != 0 || !SizeOut)
		{
			m_LastWin32Error = conversion_ret;

			delete[] UrlW;

			return SYMBOL_ERR_STRING_CONVERSION_FAILED;
		}

		printf("Age  = %08X\n", pdb_info->Age);
		printf("Name = %s\n", pdb_info->PdbFileName);
		printf("URL  = %ls\n", UrlW);

		auto hr = URLDownloadToFileW(nullptr, UrlW, m_szPdbPath.c_str(), NULL, nullptr);

		delete[] UrlW;

		if (FAILED(hr))
		{
			m_LastWin32Error = hr;

			VirtualFree(pLocalImageBase, 0, MEM_RELEASE);

			delete[] pRawData;

			return SYMBOL_ERR_DOWNLOAD_FAILED;
		}
	}

	VirtualFree(pLocalImageBase, 0, MEM_RELEASE);

	delete[] pRawData;

	if (!Filesize)
	{
		if (!GetFileAttributesExW(m_szPdbPath.c_str(), GetFileExInfoStandard, &file_attr_data))
		{
			m_LastWin32Error = GetLastError();

			return SYMBOL_ERR_CANT_ACCESS_PDB_FILE;
		}

		Filesize = file_attr_data.nFileSizeLow;
	}

	HANDLE hPdbFile = CreateFileW(m_szPdbPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, NULL, nullptr);
	if (hPdbFile == INVALID_HANDLE_VALUE)
	{
		m_LastWin32Error = GetLastError();

		return SYMBOL_ERR_CANT_OPEN_PDB_FILE;
	}

	m_hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
	if (!m_hProcess)
	{
		m_LastWin32Error = GetLastError();

		CloseHandle(hPdbFile);

		return SYMBOL_ERR_CANT_OPEN_PROCESS;
	}

	if (!SymInitializeW(m_hProcess, m_szPdbPath.c_str(), FALSE))
	{
		m_LastWin32Error = GetLastError();

		CloseHandle(m_hProcess);
		CloseHandle(hPdbFile);

		return SYMBOL_ERR_SYM_INIT_FAIL;
	}

	SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_AUTO_PUBLICS);

	m_SymbolTable = SymLoadModuleExW(m_hProcess, nullptr, m_szPdbPath.c_str(), nullptr, SymbolBase, Filesize, nullptr, NULL);
	if (!m_SymbolTable)
	{
		m_LastWin32Error = GetLastError();

		SymCleanup(m_hProcess);

		CloseHandle(m_hProcess);
		CloseHandle(hPdbFile);

		return SYMBOL_ERR_SYM_LOAD_TABLE;
	}

	if (pdb_path_out)
	{
		*pdb_path_out = m_szPdbPath;
	}

	m_Initialized = true;

	return SYMBOL_ERR_SUCCESS;
}

DWORD SYMBOL_PARSER::GetSymbolAddress(std::string szSymbolName, DWORD & RvaOut)
{
	if (!m_Initialized)
	{
		return SYMBOL_ERR_NOT_INITIALIZED;
	}

	SYMBOL_INFO si{ 0 };
	si.SizeOfStruct = sizeof(SYMBOL_INFO);
	if (!SymFromName(m_hProcess, szSymbolName.c_str(), &si))
	{
		m_LastWin32Error = GetLastError();

		return SYMBOL_ERR_SYMBOL_SEARCH_FAILED;
	}

	RvaOut = (DWORD)(si.Address - si.ModBase);

	return SYMBOL_ERR_SUCCESS;
}

DWORD SYMBOL_PARSER::GetSymbolName(DWORD RvaIn, std::string & szSymbolNameOut)
{
	if (!m_Initialized)
	{
		return SYMBOL_ERR_NOT_INITIALIZED;
	}

	char raw_data[0x1000]{ 0 };
	SYMBOL_INFO * psi = (SYMBOL_INFO *)raw_data;
	psi->SizeOfStruct	= sizeof(SYMBOL_INFO);
	psi->MaxNameLen		= 0x1000;

	if (!SymFromAddr(m_hProcess, (DWORD64)SymbolBase + RvaIn, nullptr, psi))
	{
		m_LastWin32Error = GetLastError();

		return SYMBOL_ERR_SYMBOL_SEARCH_FAILED;
	}

	szSymbolNameOut = psi->Name;

	return SYMBOL_ERR_SUCCESS;
}

BOOL __stdcall EnumerateSymbolsCallback(SYMBOL_INFO * pSymInfo, ULONG SymbolSize, void * UserContext)
{
	UNREFERENCED_PARAMETER(SymbolSize);

	auto * info = ReCa<std::vector<SYM_INFO_COMPACT> *>(UserContext);

	info->push_back(SYM_INFO_COMPACT{ pSymInfo->Name, (DWORD)((pSymInfo->Address - SYMBOL_PARSER::SymbolBase) & 0xFFFFFFFF) });

	return TRUE;
}

DWORD SYMBOL_PARSER::EnumSymbols(std::string szFilter, std::vector<SYM_INFO_COMPACT> & info)
{
	if (!SymEnumSymbols(m_hProcess, (DWORD64)SymbolBase, szFilter.c_str(), EnumerateSymbolsCallback, &info))
	{
		m_LastWin32Error = GetLastError();

		return SYMBOL_ERR_SYM_ENUM_SYMBOLS_FAILED;
	}

	return SYMBOL_ERR_SUCCESS;
}

DWORD SYMBOL_PARSER::EnumSymbolsInRange(std::string szFilter, DWORD min_rva, DWORD max_rva, std::vector<SYM_INFO_COMPACT> & info)
{
	auto ret = EnumSymbols(szFilter, info);
	if (ret != SYMBOL_ERR_SUCCESS)
	{
		return ret;
	}

	for (int i = 0; i < info.size(); )
	{
		if (info[i].RVA >= max_rva && max_rva != 0xFFFFFFFF || info[i].RVA < min_rva && min_rva != 0xFFFFFFFF)
		{
			info.erase(info.begin() + i);
		}
		else
		{
			++i;
		}
	}

	return SYMBOL_ERR_SUCCESS;
}

DWORD SYMBOL_PARSER::EnumSymbolsEx(std::string szFilter, std::vector<SYM_INFO_COMPACT> & info, SYMBOL_SORT sort, bool ascending)
{
	DWORD ret = EnumSymbols(szFilter, info);
	if (ret != SYMBOL_ERR_SUCCESS)
	{
		return ret;
	}

	if (sort == SYMBOL_SORT::None)
	{
		return ret;
	}

	switch (sort)
	{
		case SYMBOL_PARSER::SYMBOL_SORT::Rva:
			std::sort(info.begin(), info.end(), sort_symbols_rva);
			break;

		case SYMBOL_PARSER::SYMBOL_SORT::Name:
			std::sort(info.begin(), info.end(), sort_symbols_name);
			break;

		case SYMBOL_PARSER::SYMBOL_SORT::Length:
			std::sort(info.begin(), info.end(), sort_symbols_length);
			break;

		default:
			break;
	}

	if (!ascending)
	{
		std::reverse(info.begin(), info.end());
	}

	return SYMBOL_ERR_SUCCESS;
}

DWORD SYMBOL_PARSER::EnumSymbolsInRangeEx(std::string szFilter, DWORD min_rva, DWORD max_rva, std::vector<SYM_INFO_COMPACT> & info, SYMBOL_SORT sort, bool ascending)
{
	DWORD ret = EnumSymbolsInRange(szFilter, min_rva, max_rva, info);
	if (ret != SYMBOL_ERR_SUCCESS)
	{
		return ret;
	}

	if (sort == SYMBOL_SORT::None)
	{
		return ret;
	}

	switch (sort)
	{
		case SYMBOL_PARSER::SYMBOL_SORT::Rva:
			std::sort(info.begin(), info.end(), sort_symbols_rva);
			break;

		case SYMBOL_PARSER::SYMBOL_SORT::Name:
			std::sort(info.begin(), info.end(), sort_symbols_name);
			break;

		case SYMBOL_PARSER::SYMBOL_SORT::Length:
			std::sort(info.begin(), info.end(), sort_symbols_length);
			break;

		default:
			break;
	}

	if (!ascending)
	{
		std::reverse(info.begin(), info.end());
	}

	return SYMBOL_ERR_SUCCESS;
}

DWORD SYMBOL_PARSER::LastError()
{
	return m_LastWin32Error;
}