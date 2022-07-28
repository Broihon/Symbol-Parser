# Symbol Parser

A small class to parse debug info from PEs, download their respective PDBs from the Microsoft Public Symbol Server 
and calculate RVAs of functions.

----

x86 builds are compatible with x64 PEs

x64 builds are compatible with x86 PEs

Keep in mind to use SysWOW64 or Sysnative (instead of System32) when trying to load a system dll crossplatform.

Check main.cpp for an example.

Shoutout to mambda and their [pdb-parser](https://bitbucket.org/mambda/pdb-parser/src/master/) project which is a great resource to learn from.

----

## Functions

### Initialize
This function initializes the instance of the symbol parser.

```cpp
DWORD Initialize(
	const std::wstring & szModulePath, 
	const std::wstring & path, 
	std::wstring * pdb_path_out, 
	bool Redownload = false
);
```
##### Parameters
```szModulePath```\
The path to the target module file whose symbols are to be loaded. This has to be a full path starting with the drive letter.

```path```\
The path to the directory where the PDB file will be downloaded to.

```pdb_path_out```\
A pointer to a string variable that will receive the full path after the PDB is downloaded. This parameter is optional can be set to 0.

```Redownload```\
If set to true the symbol parser will redownload the PDB even if an already existing file matches the specified target module.

### GetSymbolAddress
This function resolves the relative virtual address of a symbol by its name.
```cpp
DWORD GetSymbolAddress(
	std::string szSymbolName, 
	DWORD & RvaOut
);
```
##### Parameters
```szSymbolName```\
An ANSI string object which contains the symbol name.

```RvaOut```\
A reference to a DWORD variable to receive the relative virtual address of the resolved symbol.

### GetSymbolName
This function resolves the name of a symbol by its relative virtual address.
```cpp
DWORD GetSymbolName(
	DWORD RvaIn, 
	std::string & szSymbolNameOut
);
```
##### Parameters
```RvaIn```\
The relative virtual address of the symbol.

```szSymbolNameOut```\
A reference to a string object that will receive the full name of the symbol.

### EnumSymbols
This will enumerate all available symbols from a PDB file.
```cpp
DWORD EnumSymbols(
	std::string szFilter,
	std::vector<SYM_INFO_COMPACT> & info)
);
```
##### Parameters
```szFilter```\
An ANSI string object which can be used to filter the enumerated symbols. See [SysEnumSymbols](https://docs.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-symenumsymbols)->Mask for more information.

```info```\
A reference to a vector of SYM_INFO_COMPACT structures which will be filled with information of the enumerated symbols.

### EnumSymbolsInRange
This will enumerate all symbols in the given range from a PDB file.
```cpp
DWORD EnumSymbolsInRange(
	std::string szFilter,
	DWORD min_rva, 
	DWORD max_rva,
	std::vector<SYM_INFO_COMPACT> & info)
);
```
##### Parameters
```szFilter```\
An ANSI string object which can be used to filter the enumerated symbols. See [SysEnumSymbols](https://docs.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-symenumsymbols)->Mask for more information.

```min_rva```\
The lower bound of the range.

```max_rva```\
The upper bound of the range.

```info```\
A reference to a vector of SYM_INFO_COMPACT structures which will be filled with information of the enumerated symbols.

### EnumSymbolsEx
This will enumerate all available symbols from a PDB file and then sort the enumerated data.
```cpp
DWORD EnumSymbolsEx(
	std::string szFilter,
	std::vector<SYM_INFO_COMPACT> & info), 
	SYMBOL_SORT sort = SYMBOL_SORT::None, 
	bool ascending = true
);
```
##### Parameters
```szFilter```\
An ANSI string object which can be used to filter the enumerated symbols. See [SysEnumSymbols](https://docs.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-symenumsymbols)->Mask for more information.

```info```\
A reference to a vector of SYM_INFO_COMPACT structures which will be filled with information of the enumerated symbols.

```sort```\
This parameter defines how the vector will be sorted. The possible values are:
* ```SYMBOL_SORT::None```: The vector will not be sorted.
* ```SYMBOL_SORT::Rva```: The vector will be sorted by the relative virtual address of the symbol.
* ```SYMBOL_SORT::Name```: The vector will be sorted alphabetically by the symbol name.
* ```SYMBOL_SORT::Length```: The vector will be sorted by the length of the symbol name.

```ascending```\
If true the vector is sorted ascendingly.
If fals the vector is sorted in descending order.

### EnumSymbolsInRangeEx
This will enumerate all available symbols in the given range from a PDB file and then sort the enumerated data.
```cpp
DWORD EnumSymbolsInRangeEx(
	std::string szFilter,
	DWORD min_rva, 
	DWORD max_rva, 
	std::vector<SYM_INFO_COMPACT> & info), 
	SYMBOL_SORT sort = SYMBOL_SORT::None, 
	bool ascending = true
);
```
##### Parameters
```szFilter```\
An ANSI string object which can be used to filter the enumerated symbols. See [SysEnumSymbols](https://docs.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-symenumsymbols)->Mask for more information.

```min_rva```\
The lower bound of the range.

```max_rva```\
The upper bound of the range.

```info```\
A reference to a vector of SYM_INFO_COMPACT structures which will be filled with information of the enumerated symbols.

```sort```\
This parameter defines how the vector will be sorted. The possible values are:
* ```SYMBOL_SORT::None```: The vector will not be sorted.
* ```SYMBOL_SORT::Rva```: The vector will be sorted by the relative virtual address of the symbol.
* ```SYMBOL_SORT::Name```: The vector will be sorted alphabetically by the symbol name.
* ```SYMBOL_SORT::Length```: The vector will be sorted by the length of the symbol name.

```ascending```\
If true the vector is sorted ascendingly.
If fals the vector is sorted in descending order.

## Return value
On success all of the above functions return SYM_ERROR_SUCCESS (0).\
If a function fails it returns one of the following error codes.
* ```SYMBOL_ERR_ALREADY_INITIALIZED (0x00000001)```\
This instance of the symbol parser is already initialized.
* ```SYMBOL_ERR_CANT_OPEN_MODULE (0x00000002)```\
Unable to open the specified file.
* ```SYMBOL_ERR_FILE_SIZE_IS_NULL (0x00000003)```\
The size of the specified file is invalid.
* ```SYMBOL_ERR_CANT_ALLOC_MEMORY_NEW (0x00000004)```\
Memory allocation using operator new failed.
* ```SYMBOL_ERR_INVALID_FILE_ARCHITECTURE (0x00000005)```\
The architecture of the specified file not compatible with the symbol parser.
* ```SYMBOL_ERR_CANT_ALLOC_MEMORY (0x00000006)```\
Memory allocation using [VirtualAlloc](https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc) failed. Call SYMBOL_PARSER::LastError to retrieve a win32 error code.
* ```SYMBOL_ERR_NO_PDB_DEBUG_DATA (0x00000007)```\
The specified file does not contain any PDB debug data in its debug directory.
* ```SYMBOL_ERR_PATH_DOESNT_EXIST (0x00000008)```\
Unable to create or open existing path using [CreateDirectoryA](https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createdirectorya). Call SYMBOL_PARSER::LastError to retrieve a win32 error code.
* ```SYMBOL_ERR_CANT_CREATE_DIRECTORY (0x00000009)```\
Unable to create or open existing path to store the PDB file in using [CreateDirectoryA](https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createdirectorya). Call SYMBOL_PARSER::LastError to retrieve a win32 error code.
* ```SYMBOL_ERR_CANT_CONVERT_PDB_GUID (0x0000000B)```\
Failed to convert GUID using [StringFromGUID2](https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-stringfromguid2). Call SYMBOL_PARSER::LastError to retrieve a win32 error code.
* ```SYMBOL_ERR_GUID_TO_ANSI_FAILED (0x0000000C)```\
Failed to convert unicode GUID to ANSI using [wcstombs_s](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/wcstombs-s-wcstombs-s-l?view=msvc-170).
* ```SYMBOL_ERR_DOWNLOAD_FAILED (0x0000000D)```\
[URLDownloadToFileA](https://docs.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/ms775123(v=vs.85)) failed to download the PDB file. Call SYMBOL_PARSER::LastError to retrieve a HRESULT error code.
* ```SYMBOL_ERR_CANT_ACCESS_PDB_FILE (0x0000000E)```\
Unable to access the downloaded file using [GetFileAttributesExA](https://docs.microsoft.com/en-us/previous-versions/windows/embedded/ms890909(v=msdn.10)). Call SYMBOL_PARSER::LastError to retrieve a win32 error code.
* ```SYMBOL_ERR_CANT_OPEN_PDB_FILE (0x0000000F)```\
Unable to open the PDB file using [CreateFileA](https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea). Call SYMBOL_PARSER::LastError to retrieve a win32 error code.
* ```SYMBOL_ERR_IVNALID_SYMBOL_NAME (0x00000010)```\
szSymbolName can't be 0.
* ```SYMBOL_ERR_CANT_OPEN_PROCESS (0x00000011)```\
Failed to create a handle to the current process using [OpenProcess](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-openprocess). Call SYMBOL_PARSER::LastError to retrieve a win32 error code.
* ```SYMBOL_ERR_SYM_INIT_FAIL (0x00000012)```\
The call to [SymInitialize](https://docs.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-syminitialize) failed. Call SYMBOL_PARSER::LastError to retrieve a win32 error code.
* ```SYMBOL_ERR_SYM_LOAD_TABLE (0x00000013)```\
The call to [SymLoadModuleEx](https://docs.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-symloadmoduleex) failed. Call SYMBOL_PARSER::LastError to retrieve a win32 error code.
* ```SYMBOL_ERR_NOT_INITIALIZED (0x00000014)```\
This instance of the symbol isn't initialized. Successfully call SYMBOL_PARSER::Initialize before using another function.
* ```SYMBOL_ERR_SYMBOL_SEARCH_FAILED (0x00000015)```\
[SymFromName](https://docs.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-symfromname) or [SymFromAddr](https://docs.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-symfromaddr) was unable to locate the specified symbol. Call SYMBOL_PARSER::LastError to retrieve a win32 error code.
* ```SYMBOL_ERR_SYM_ENUM_SYMBOLS_FAILED (0x00000017)```\
[SymEnumSymbols](https://docs.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-symenumsymbols) failed. Call SYMBOL_PARSER::LastError to retrieve a win32 error code.
* ```SYMBOL_ERR_STRING_CONVERSION_FAILED (0x00000018)```\
Failed to convert string from ANSI to unicode using [mbstowcs_s](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/mbstowcs-s-mbstowcs-s-l?view=msvc-170).