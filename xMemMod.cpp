/**
 * @file xMemMod.cpp
 * @brief MemoryModule - Реализация статической библиотеки для загрузки DLL из памяти
 * @details Современная реализация C++17/20 для Windows x86/x64
 * @author Platon
 * @version 3.0.0
 * @date 2025
 */

#include "xMemMod.h"
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace MemoryModule {

// Архитектурные константы
#ifdef _WIN64
    constexpr WORD HOST_MACHINE = IMAGE_FILE_MACHINE_AMD64;
#else
    constexpr WORD HOST_MACHINE = IMAGE_FILE_MACHINE_I386;
#endif

// Конструктор
MemoryModule::MemoryModule() noexcept
    : code_base_(nullptr)
    , image_size_(0)
    , headers_(nullptr, [](IMAGE_NT_HEADERS*) {})
    , is_loaded_(false)
    , is_64bit_(false)
    , export_list_built_(false)
    , page_size_(0) {
    
    SYSTEM_INFO sys_info;
    GetNativeSystemInfo(&sys_info);
    page_size_ = sys_info.dwPageSize;
}

// Деструктор
MemoryModule::~MemoryModule() noexcept {
    Unload();
}

// Move конструктор
MemoryModule::MemoryModule(MemoryModule&& other) noexcept
    : code_base_(std::exchange(other.code_base_, nullptr))
    , image_size_(std::exchange(other.image_size_, 0))
    , headers_(std::move(other.headers_))
    , is_loaded_(other.is_loaded_.exchange(false))
    , is_64bit_(other.is_64bit_.exchange(false))
    , export_list_(std::move(other.export_list_))
    , export_list_built_(other.export_list_built_.exchange(false))
    , page_size_(std::exchange(other.page_size_, 0)) {
}

// Move оператор присваивания
MemoryModule& MemoryModule::operator=(MemoryModule&& other) noexcept {
    if (this != &other) {
        Unload();
        
        code_base_ = std::exchange(other.code_base_, nullptr);
        image_size_ = std::exchange(other.image_size_, 0);
        headers_ = std::move(other.headers_);
        is_loaded_ = other.is_loaded_.exchange(false);
        is_64bit_ = other.is_64bit_.exchange(false);
        export_list_ = std::move(other.export_list_);
        export_list_built_ = other.export_list_built_.exchange(false);
        page_size_ = std::exchange(other.page_size_, 0);
    }
    return *this;
}

// Основной метод загрузки
bool MemoryModule::LoadFromMemory(const void* data, size_t size) noexcept {
    try {
        if (!data || size == 0) {
            return false;
        }
        
        // Освобождаем предыдущий модуль
        Unload();
        
        // Загружаем PE
        if (!LoadPE(data, size)) {
            return false;
        }
        
        is_loaded_.store(true);
        return true;
        
    } catch (...) {
        return false;
    }
}

// Получение адреса функции
FARPROC MemoryModule::GetProcAddress(const char* name) const noexcept {
    try {
        if (!IsValid() || !name) {
            return nullptr;
        }
        
        // Сначала пытаемся найти по имени
        auto exports = GetExportList();
        for (const auto& exp : exports) {
            if (exp.name == name) {
                return exp.address;
            }
        }
        
        // Если не найдено по имени, пытаемся найти по ординалу
        if (std::all_of(name, name + strlen(name), ::isdigit)) {
            UInt16 ordinal = static_cast<UInt16>(std::stoi(name));
            return GetProcAddressByOrdinal(ordinal);
        }
        
        return nullptr;
        
    } catch (...) {
        return nullptr;
    }
}

// Получение списка всех экспортов с готовыми указателями
std::vector<ExportInfo> MemoryModule::GetExportList() const noexcept {
    std::lock_guard<std::mutex> lock(export_mutex_);
    
    if (!export_list_built_.load()) {
        BuildExportTable();
    }
    
    return export_list_;
}

// Освобождение ресурсов
bool MemoryModule::Unload() noexcept {
    try {
        if (!IsValid()) {
            return true;
        }
        
        // Вызываем DLL_PROCESS_DETACH если это DLL
        if (is_loaded_.load() && headers_) {
            if (headers_->FileHeader.Characteristics & IMAGE_FILE_DLL) {
                using DllEntryProc = BOOL(WINAPI*)(HINSTANCE, DWORD, LPVOID);
                DllEntryProc dll_entry = reinterpret_cast<DllEntryProc>(
                    static_cast<char*>(code_base_) + headers_->OptionalHeader.AddressOfEntryPoint);
                dll_entry(static_cast<HINSTANCE>(code_base_), DLL_PROCESS_DETACH, nullptr);
            }
        }
        
        // Очищаем кэш экспортов
        export_list_.clear();
        export_list_built_.store(false);
        
        // Освобождаем память
        if (code_base_) {
            VirtualFree(code_base_, 0, MEM_RELEASE);
            code_base_ = nullptr;
        }
        
        image_size_ = 0;
        headers_.reset();
        is_loaded_.store(false);
        is_64bit_.store(false);
        
        return true;
        
    } catch (...) {
        return false;
    }
}

// Проверка архитектуры
bool MemoryModule::Is64Bit() const noexcept {
    return is_64bit_.load();
}

// Получение имени модуля
std::string MemoryModule::GetModuleName() const noexcept {
    try {
        if (!IsValid()) {
            return "";
        }
        
        auto exports = GetExportList();
        if (exports.empty()) {
            return "Unknown";
        }
        
        // Возвращаем имя первой экспортируемой функции как имя модуля
        return exports[0].name;
        
    } catch (...) {
        return "";
    }
}

// Получение количества экспортов
UInt32 MemoryModule::GetExportCount() const noexcept {
    try {
        auto exports = GetExportList();
        return static_cast<UInt32>(exports.size());
    } catch (...) {
        return 0;
    }
}

// Получение адреса функции по ординалу
FARPROC MemoryModule::GetProcAddressByOrdinal(UInt16 ordinal) const noexcept {
    try {
        if (!IsValid()) {
            return nullptr;
        }
        
        auto exports = GetExportList();
        for (const auto& exp : exports) {
            if (exp.ordinal == ordinal) {
                return exp.address;
            }
        }
        
        return nullptr;
        
    } catch (...) {
        return nullptr;
    }
}

// Получение имени функции по ординалу
std::string MemoryModule::GetFunctionName(UInt16 ordinal) const noexcept {
    try {
        if (!IsValid()) {
            return "";
        }
        
        auto exports = GetExportList();
        for (const auto& exp : exports) {
            if (exp.ordinal == ordinal) {
                return exp.name;
            }
        }
        
        return "";
        
    } catch (...) {
        return "";
    }
}

// Получение ординала функции по имени
UInt16 MemoryModule::GetFunctionOrdinal(const char* name) const noexcept {
    try {
        if (!IsValid() || !name) {
            return 0;
        }
        
        auto exports = GetExportList();
        for (const auto& exp : exports) {
            if (exp.name == name) {
                return static_cast<UInt16>(exp.ordinal);
            }
        }
        
        return 0;
        
    } catch (...) {
        return 0;
    }
}

// Загрузка PE файла
bool MemoryModule::LoadPE(const void* data, size_t size) noexcept {
    try {
        // Валидация PE
        if (!IsValidPE(data, size)) {
            return false;
        }
        
        const IMAGE_DOS_HEADER* dos_header = static_cast<const IMAGE_DOS_HEADER*>(data);
        const IMAGE_NT_HEADERS* old_headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(
            static_cast<const char*>(data) + dos_header->e_lfanew);
        
        if (!IsSupportedArchitecture(old_headers)) {
            return false;
        }
        
        // Определяем архитектуру
        is_64bit_.store(old_headers->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64);
        
        // Вычисляем размер образа
        size_t image_size = old_headers->OptionalHeader.SizeOfImage;
        size_t aligned_image_size = AlignValue(image_size, page_size_);
        
        // Выделяем память
        code_base_ = VirtualAlloc(
            reinterpret_cast<void*>(old_headers->OptionalHeader.ImageBase),
            aligned_image_size,
            MEM_RESERVE | MEM_COMMIT,
            PAGE_READWRITE
        );
        
        if (!code_base_) {
            code_base_ = VirtualAlloc(
                nullptr,
                aligned_image_size,
                MEM_RESERVE | MEM_COMMIT,
                PAGE_READWRITE
            );
        }
        
        if (!code_base_) {
            return false;
        }
        
        image_size_ = aligned_image_size;
        
        // Копируем заголовки
        memcpy(code_base_, data, old_headers->OptionalHeader.SizeOfHeaders);
        
        // Создаем новые заголовки
        headers_ = std::unique_ptr<IMAGE_NT_HEADERS, void(*)(IMAGE_NT_HEADERS*)>(
            reinterpret_cast<IMAGE_NT_HEADERS*>(
                static_cast<char*>(code_base_) + dos_header->e_lfanew),
            [](IMAGE_NT_HEADERS*) {}
        );
        
        headers_->OptionalHeader.ImageBase = reinterpret_cast<UInt64>(code_base_);
        
        // Копируем секции
        if (!CopySections(data, old_headers)) {
            return false;
        }
        
        // Выполняем релокации
        std::ptrdiff_t delta = reinterpret_cast<std::ptrdiff_t>(code_base_) - 
                              old_headers->OptionalHeader.ImageBase;
        if (delta != 0) {
            if (!PerformBaseRelocation(delta)) {
                return false;
            }
        }
        
        // Строим таблицу импортов
        if (!BuildImportTable()) {
            return false;
        }
        
        // Финализируем секции
        if (!FinalizeSections()) {
            return false;
        }
        
        // Выполняем TLS
        if (!ExecuteTLS()) {
            return false;
        }
        
        // Вызываем точку входа
        if (!CallEntryPoint()) {
            return false;
        }
        
        return true;
        
    } catch (...) {
        return false;
    }
}

// Копирование секций
bool MemoryModule::CopySections(const void* data, const IMAGE_NT_HEADERS* old_headers) noexcept {
    try {
        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(old_headers);
        
        for (UInt16 i = 0; i < old_headers->FileHeader.NumberOfSections; ++i, ++section) {
            if (section->SizeOfRawData == 0) {
                continue;
            }
            
            void* dest = static_cast<char*>(code_base_) + section->VirtualAddress;
            const void* src = static_cast<const char*>(data) + section->PointerToRawData;
            memcpy(dest, src, section->SizeOfRawData);
        }
        
        return true;
        
    } catch (...) {
        return false;
    }
}

// Финализация секций
bool MemoryModule::FinalizeSections() noexcept {
    try {
        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(headers_.get());
        
        for (UInt16 i = 0; i < headers_->FileHeader.NumberOfSections; ++i, ++section) {
            if (section->SizeOfRawData == 0) {
                continue;
            }
            
            void* address = static_cast<char*>(code_base_) + section->VirtualAddress;
            void* aligned_address = AlignAddress(address, page_size_);
            size_t size = section->Misc.VirtualSize;
            
            if (!SetSectionProtection(aligned_address, size, section->Characteristics)) {
                return false;
            }
        }
        
        return true;
        
    } catch (...) {
        return false;
    }
}

// Выполнение релокаций
bool MemoryModule::PerformBaseRelocation(std::ptrdiff_t delta) noexcept {
    try {
        if (delta == 0) {
            return true;
        }
        
        auto* reloc_dir = &headers_->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if (reloc_dir->VirtualAddress == 0) {
            return true;
        }
        
        auto* reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
            static_cast<unsigned char*>(code_base_) + reloc_dir->VirtualAddress);
        auto* reloc_end = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
            static_cast<unsigned char*>(code_base_) + reloc_dir->VirtualAddress + reloc_dir->Size);
        
        while (reloc < reloc_end) {
            auto* reloc_data = reinterpret_cast<UInt16*>(reloc + 1);
            auto* reloc_data_end = reinterpret_cast<UInt16*>(
                reinterpret_cast<unsigned char*>(reloc) + reloc->SizeOfBlock);
            
            while (reloc_data < reloc_data_end) {
                UInt16 type = *reloc_data >> 12;
                UInt16 offset = *reloc_data & 0xFFF;
                
                if (type == IMAGE_REL_BASED_HIGHLOW || type == IMAGE_REL_BASED_DIR64) {
                    auto* patch_address = reinterpret_cast<std::uintptr_t*>(
                        static_cast<unsigned char*>(code_base_) + reloc->VirtualAddress + offset);
                    *patch_address += delta;
                }
                
                ++reloc_data;
            }
            
            reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reloc_data);
        }
        
        return true;
        
    } catch (...) {
        return false;
    }
}

// Построение таблицы импортов
bool MemoryModule::BuildImportTable() noexcept {
    try {
        auto* import_dir = &headers_->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (import_dir->VirtualAddress == 0) {
            return true;
        }
        
        auto* import_desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
            static_cast<char*>(code_base_) + import_dir->VirtualAddress);
        
        while (import_desc->Name != 0) {
            const char* dll_name = reinterpret_cast<const char*>(
                static_cast<char*>(code_base_) + import_desc->Name);
            
            HMODULE dll_handle = LoadLibraryA(dll_name);
            if (!dll_handle) {
                return false;
            }
            
            auto* thunk_data = reinterpret_cast<IMAGE_THUNK_DATA*>(
                static_cast<char*>(code_base_) + import_desc->FirstThunk);
            auto* orig_thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
                static_cast<char*>(code_base_) + import_desc->OriginalFirstThunk);
            
            if (orig_thunk == 0) {
                orig_thunk = thunk_data;
            }
            
            while (orig_thunk->u1.AddressOfData != 0) {
                void* func_address = nullptr;
                
                if (orig_thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                    UInt16 ordinal = static_cast<UInt16>(orig_thunk->u1.Ordinal & 0xFFFF);
                    func_address = reinterpret_cast<void*>(::GetProcAddress(dll_handle, MAKEINTRESOURCEA(ordinal)));
                } else {
                    auto* import_by_name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                        static_cast<unsigned char*>(code_base_) + orig_thunk->u1.AddressOfData);
                    const char* func_name = import_by_name->Name;
                    func_address = reinterpret_cast<void*>(::GetProcAddress(dll_handle, func_name));
                }
                
                if (!func_address) {
                    return false;
                }
                
                thunk_data->u1.Function = reinterpret_cast<std::uintptr_t>(func_address);
                ++thunk_data;
                ++orig_thunk;
            }
            
            ++import_desc;
        }
        
        return true;
        
    } catch (...) {
        return false;
    }
}

// Построение таблицы экспортов
bool MemoryModule::BuildExportTable() const noexcept {
    try {
        if (export_list_built_.load()) {
            return true;
        }
        
        export_list_.clear();
        
        auto* export_dir = &headers_->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        
        if (export_dir->VirtualAddress == 0) {
            export_list_built_.store(true);
            return true;
        }
        
        auto* export_table = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(
            static_cast<char*>(code_base_) + export_dir->VirtualAddress);
        
        if (export_table->NumberOfFunctions == 0) {
            export_list_built_.store(true);
            return true;
        }
        
        auto* name_rva = reinterpret_cast<UInt32*>(
            static_cast<char*>(code_base_) + export_table->AddressOfNames);
        auto* ordinal_rva = reinterpret_cast<UInt16*>(
            static_cast<char*>(code_base_) + export_table->AddressOfNameOrdinals);
        auto* function_rva = reinterpret_cast<UInt32*>(
            static_cast<char*>(code_base_) + export_table->AddressOfFunctions);
        
        for (UInt32 i = 0; i < export_table->NumberOfNames; ++i) {
            const char* name = reinterpret_cast<const char*>(
                static_cast<char*>(code_base_) + name_rva[i]);
            UInt16 ordinal = ordinal_rva[i];
            void* address = static_cast<char*>(code_base_) + function_rva[ordinal];
            
            UInt32 func_rva = function_rva[ordinal];
            UInt32 final_ordinal = ordinal + export_table->Base;
            
            ExportInfo info(final_ordinal, func_rva, export_table->Base, 
                           static_cast<UInt32>(reinterpret_cast<uintptr_t>(address)), name, 
                           reinterpret_cast<FARPROC>(address));
            
            export_list_.push_back(info);
        }
        
        export_list_built_.store(true);
        return true;
        
    } catch (...) {
        return false;
    }
}

// Выполнение TLS
bool MemoryModule::ExecuteTLS() noexcept {
    try {
        if (headers_->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress == 0) {
            return true;
        }
        
        auto* tls = reinterpret_cast<IMAGE_TLS_DIRECTORY*>(
            static_cast<char*>(code_base_) + headers_->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);
        
        if (tls->AddressOfCallBacks == 0) {
            return true;
        }
        
        auto** callbacks = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(tls->AddressOfCallBacks);
        for (; *callbacks; ++callbacks) {
            (*callbacks)(code_base_, DLL_PROCESS_ATTACH, nullptr);
        }
        
        return true;
        
    } catch (...) {
        return false;
    }
}

// Вызов точки входа
bool MemoryModule::CallEntryPoint() noexcept {
    try {
        if (headers_->OptionalHeader.AddressOfEntryPoint == 0) {
            return true;
        }
        
        if (headers_->FileHeader.Characteristics & IMAGE_FILE_DLL) {
            using DllEntryProc = BOOL(WINAPI*)(HINSTANCE, DWORD, LPVOID);
            DllEntryProc dll_entry = reinterpret_cast<DllEntryProc>(
                static_cast<char*>(code_base_) + headers_->OptionalHeader.AddressOfEntryPoint);
            return dll_entry(static_cast<HINSTANCE>(code_base_), DLL_PROCESS_ATTACH, nullptr);
        }
        
        return true;
        
    } catch (...) {
        return false;
    }
}

// Валидация PE
bool MemoryModule::IsValidPE(const void* data, size_t size) const noexcept {
    if (!data || size < sizeof(IMAGE_DOS_HEADER)) {
        return false;
    }
    
    const IMAGE_DOS_HEADER* dos_header = static_cast<const IMAGE_DOS_HEADER*>(data);
    if (!PEUtils::IsValidDOSHeader(dos_header)) {
        return false;
    }
    
    if (size < static_cast<size_t>(dos_header->e_lfanew + sizeof(IMAGE_NT_HEADERS))) {
        return false;
    }
    
    const IMAGE_NT_HEADERS* headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        static_cast<const char*>(data) + dos_header->e_lfanew);
    
    return PEUtils::IsValidNTHeaders(headers);
}

// Проверка поддерживаемой архитектуры
bool MemoryModule::IsSupportedArchitecture(const IMAGE_NT_HEADERS* headers) const noexcept {
#ifdef _WIN64
    return headers && headers->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64;
#else
    return headers && headers->FileHeader.Machine == IMAGE_FILE_MACHINE_I386;
#endif
}

// Выравнивание адреса
void* MemoryModule::AlignAddress(void* address, size_t alignment) const noexcept {
    return reinterpret_cast<void*>(
        (reinterpret_cast<uintptr_t>(address) & ~(alignment - 1)));
}

// Выравнивание значения
size_t MemoryModule::AlignValue(size_t value, size_t alignment) const noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

// Установка защиты секции
bool MemoryModule::SetSectionProtection(void* address, size_t size, DWORD characteristics) noexcept {
    try {
        DWORD protect = PAGE_NOACCESS;
        
        if (characteristics & IMAGE_SCN_MEM_EXECUTE) {
            protect = (characteristics & IMAGE_SCN_MEM_WRITE) ? 
                     PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
        } else if (characteristics & IMAGE_SCN_MEM_WRITE) {
            protect = PAGE_READWRITE;
        } else if (characteristics & IMAGE_SCN_MEM_READ) {
            protect = PAGE_READONLY;
        }
        
        DWORD old_protect;
        return VirtualProtect(address, size, protect, &old_protect) != 0;
        
    } catch (...) {
        return false;
    }
}

// Создание информации об экспорте
ExportInfo MemoryModule::CreateExportInfo(UInt32 ordinal, UInt32 rva, UInt16 ord_base, 
                                        UInt32 va, const std::string& name, FARPROC address) const noexcept {
    return ExportInfo(ordinal, rva, ord_base, va, name, address);
}

// Реализация PEUtils
namespace PEUtils {
    bool IsValidDOSHeader(const IMAGE_DOS_HEADER* header) noexcept {
        return header && header->e_magic == IMAGE_DOS_SIGNATURE;
    }
    
    bool IsValidNTHeaders(const IMAGE_NT_HEADERS* headers) noexcept {
        return headers && headers->Signature == IMAGE_NT_SIGNATURE;
    }
    
    bool IsSupportedMachine(WORD machine) noexcept {
#ifdef _WIN64
        return machine == IMAGE_FILE_MACHINE_AMD64;
#else
        return machine == IMAGE_FILE_MACHINE_I386;
#endif
    }
    
    const IMAGE_NT_HEADERS* GetNTHeaders(const void* base) noexcept {
        const IMAGE_DOS_HEADER* dos_header = static_cast<const IMAGE_DOS_HEADER*>(base);
        if (!IsValidDOSHeader(dos_header)) {
            return nullptr;
        }
        
        return reinterpret_cast<const IMAGE_NT_HEADERS*>(
            static_cast<const char*>(base) + dos_header->e_lfanew);
    }
    
    const IMAGE_SECTION_HEADER* GetFirstSection(const IMAGE_NT_HEADERS* headers) noexcept {
        if (!headers) return nullptr;
        return IMAGE_FIRST_SECTION(headers);
    }
    
    const IMAGE_SECTION_HEADER* GetSection(const IMAGE_NT_HEADERS* headers, UInt32 index) noexcept {
        if (!headers || index >= headers->FileHeader.NumberOfSections) {
            return nullptr;
        }
        
        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(headers);
        return &section[index];
    }
}

// Реализация Utils
namespace Utils {
    std::string FormatAddress(void* address) noexcept {
        std::ostringstream oss;
        oss << "0x" << std::hex << std::uppercase << address;
        return oss.str();
    }
    
    std::string FormatOrdinal(UInt16 ordinal) noexcept {
        std::ostringstream oss;
        oss << "0x" << std::hex << std::uppercase << ordinal;
        return oss.str();
    }
    
    void PrintExportTable(const std::vector<ExportInfo>& exports) noexcept {
        std::cout << "=== Export Table ===" << std::endl;
        std::cout << "№\tOrdinal\tRVA\t\tName\t\t\tAddress" << std::endl;
        std::cout << "--------------------------------------------------------" << std::endl;
        
        for (size_t i = 0; i < exports.size(); ++i) {
            const auto& exp = exports[i];
            std::cout << (i + 1) << "\t"
                      << "0x" << std::hex << exp.ordinal << "\t"
                      << "0x" << std::hex << exp.rva << "\t"
                      << exp.name << "\t\t"
                      << "0x" << std::hex << exp.address << std::dec << std::endl;
        }
    }
    
    void PrintModuleInfo(const MemoryModule& module) noexcept {
        std::cout << "=== Module Information ===" << std::endl;
        std::cout << "Base Address: " << Utils::FormatAddress(const_cast<void*>(module.GetBaseAddress())) << std::endl;
        std::cout << "Image Size: " << module.GetImageSize() << " bytes" << std::endl;
        std::cout << "Architecture: " << (module.Is64Bit() ? "x64" : "x86") << std::endl;
        std::cout << "Export Count: " << module.GetExportCount() << std::endl;
        std::cout << "Module Name: " << module.GetModuleName() << std::endl;
    }
}

} // namespace MemoryModule

// C-интерфейс
extern "C" {
    MemoryModule::MemoryModule* memory_module_create() noexcept {
        try {
            return new MemoryModule::MemoryModule();
        } catch (...) {
            return nullptr;
        }
    }
    
    void memory_module_destroy(MemoryModule::MemoryModule* module) noexcept {
        if (module) {
            delete module;
        }
    }
    
    bool memory_module_load(MemoryModule::MemoryModule* module, const void* data, size_t size) noexcept {
        if (!module) return false;
        return module->LoadFromMemory(data, size);
    }
    
    FARPROC memory_module_get_proc_address(MemoryModule::MemoryModule* module, const char* name) noexcept {
        if (!module) return nullptr;
        return module->GetProcAddress(name);
    }
    
    bool memory_module_unload(MemoryModule::MemoryModule* module) noexcept {
        if (!module) return false;
        return module->Unload();
    }
    
    bool memory_module_is_64bit(MemoryModule::MemoryModule* module) noexcept {
        if (!module) return false;
        return module->Is64Bit();
    }
    
    size_t memory_module_get_export_count(MemoryModule::MemoryModule* module) noexcept {
        if (!module) return 0;
        return module->GetExportCount();
    }
    
    void memory_module_get_export_list(MemoryModule::MemoryModule* module, 
                                      MemoryModule::ExportInfo* exports, 
                                      size_t* count) noexcept {
        if (!module || !exports || !count) {
            *count = 0;
            return;
        }
        
        auto export_list = module->GetExportList();
        *count = export_list.size();
        
        for (size_t i = 0; i < export_list.size() && i < *count; ++i) {
            exports[i] = export_list[i];
        }
    }
    
    FARPROC memory_module_get_proc_address_by_ordinal(MemoryModule::MemoryModule* module, 
                                                     MemoryModule::UInt16 ordinal) noexcept {
        if (!module) return nullptr;
        return module->GetProcAddressByOrdinal(ordinal);
    }
    
    const char* memory_module_get_function_name(MemoryModule::MemoryModule* module, 
                                               MemoryModule::UInt16 ordinal) noexcept {
        if (!module) return nullptr;
        static thread_local std::string result;
        result = module->GetFunctionName(ordinal);
        return result.c_str();
    }
    
    MemoryModule::UInt16 memory_module_get_function_ordinal(MemoryModule::MemoryModule* module, 
                                             const char* name) noexcept {
        if (!module) return 0;
        return module->GetFunctionOrdinal(name);
    }
}
