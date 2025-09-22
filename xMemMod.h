/**
 * @file xMemMod.h
 * @brief MemoryModule - Статическая библиотека для загрузки DLL из памяти
 * @details Современная реализация C++17/20 для Windows x86/x64
 * @author Platon
 * @version 3.0.0
 * @date 2025
 *
 * @license MIT License
 * @copyright Copyright (c) 2025 Platon
 *
 * Основные возможности:
 * - Загрузка DLL из массива байтов в памяти (без записи на диск)
 * - Полноценная работа с экспортируемыми функциями как с обычной DLL
 * - Автоматическое определение всех экспортируемых функций с указателями
 * - Поддержка как x86, так и x64 архитектур
 * - Совместимость с Clang (Rad Studio 13) и MSVC (Visual Studio 2022)
 */

#pragma once

// Compiler detection and compatibility
#if defined(_MSC_VER)
    #define XMEMMOD_MSVC
    #define XMEMMOD_COMPILER_VERSION _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 4251 4275)
#elif defined(__GNUC__)
    #define XMEMMOD_GCC
    #define XMEMMOD_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif defined(__clang__)
    #define XMEMMOD_CLANG
    #define XMEMMOD_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#else
    #define XMEMMOD_UNKNOWN_COMPILER
#endif

// Windows platform detection
#if !defined(_WIN32) && !defined(_WIN64)
    #error "MemoryModule requires Windows platform"
#endif

// Architecture detection
#ifdef _WIN64
    #define XMEMMOD_64BIT
    #define XMEMMOD_ARCH_BITS 64
#else
    #define XMEMMOD_32BIT
    #define XMEMMOD_ARCH_BITS 32
#endif

// Standard library includes
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

// Windows-specific includes
#include <windows.h>
#include <winnt.h>

namespace MemoryModule {

// Forward declarations
class MemoryModule;

// Portable integer types
using UInt8 = std::uint8_t;
using UInt16 = std::uint16_t;
using UInt32 = std::uint32_t;
using UInt64 = std::uint64_t;

// Расширенная структура ExportInfo с готовыми указателями
struct ExportInfo {
    UInt32 ordinal;        // Порядковый номер
    UInt32 rva;           // RVA (Relative Virtual Address)
    UInt16 ordinal_base;  // База ординалов
    UInt32 va;            // Virtual Address
    std::string name;     // Имя функции
    FARPROC address;      // ГОТОВЫЙ к использованию указатель на функцию
    
    ExportInfo() : ordinal(0), rva(0), ordinal_base(0), va(0), address(nullptr) {}
    
    ExportInfo(UInt32 ord, UInt32 func_rva, UInt16 ord_base, UInt32 virtual_addr, 
               const std::string& func_name, FARPROC func_address)
        : ordinal(ord), rva(func_rva), ordinal_base(ord_base), va(virtual_addr), 
          name(func_name), address(func_address) {}
};

// Основной класс MemoryModule
class MemoryModule {
public:
    // Конструкторы и деструкторы
    MemoryModule() noexcept;
    ~MemoryModule() noexcept;
    
    // Удаляем копирование, разрешаем перемещение
    MemoryModule(const MemoryModule&) = delete;
    MemoryModule& operator=(const MemoryModule&) = delete;
    MemoryModule(MemoryModule&& other) noexcept;
    MemoryModule& operator=(MemoryModule&& other) noexcept;
    
    // Основные методы
    bool LoadFromMemory(const void* data, size_t size) noexcept;
    FARPROC GetProcAddress(const char* name) const noexcept;
    std::vector<ExportInfo> GetExportList() const noexcept;
    bool Unload() noexcept;
    bool Is64Bit() const noexcept;
    
    // Дополнительные методы
    bool IsValid() const noexcept { return code_base_ != nullptr; }
    bool IsLoaded() const noexcept { return is_loaded_.load(); }
    const void* GetBaseAddress() const noexcept { return code_base_; }
    size_t GetImageSize() const noexcept { return image_size_; }
    
    // Получение информации о модуле
    std::string GetModuleName() const noexcept;
    UInt32 GetExportCount() const noexcept;
    
    // Поиск функций по различным критериям
    FARPROC GetProcAddressByOrdinal(UInt16 ordinal) const noexcept;
    std::string GetFunctionName(UInt16 ordinal) const noexcept;
    UInt16 GetFunctionOrdinal(const char* name) const noexcept;
    
private:
    // Основные данные
    void* code_base_;
    size_t image_size_;
    std::unique_ptr<IMAGE_NT_HEADERS, void(*)(IMAGE_NT_HEADERS*)> headers_;
    std::atomic<bool> is_loaded_;
    std::atomic<bool> is_64bit_;
    
    // Кэшированные данные экспорта
    mutable std::vector<ExportInfo> export_list_;
    mutable std::mutex export_mutex_;
    mutable std::atomic<bool> export_list_built_;
    
    // Системная информация
    UInt32 page_size_;
    
    // Внутренние методы
    bool LoadPE(const void* data, size_t size) noexcept;
    bool CopySections(const void* data, const IMAGE_NT_HEADERS* old_headers) noexcept;
    bool FinalizeSections() noexcept;
    bool PerformBaseRelocation(std::ptrdiff_t delta) noexcept;
    bool BuildImportTable() noexcept;
    bool BuildExportTable() const noexcept;
    bool ExecuteTLS() noexcept;
    bool CallEntryPoint() noexcept;
    
    // Утилиты
    bool IsValidPE(const void* data, size_t size) const noexcept;
    bool IsSupportedArchitecture(const IMAGE_NT_HEADERS* headers) const noexcept;
    void* AlignAddress(void* address, size_t alignment) const noexcept;
    size_t AlignValue(size_t value, size_t alignment) const noexcept;
    
    // Обработка секций
    bool SetSectionProtection(void* address, size_t size, DWORD characteristics) noexcept;
    
    // Обработка релокаций
    bool ProcessRelocations(std::ptrdiff_t delta) noexcept;
    
    // Обработка импортов
    bool LoadDependencies() noexcept;
    bool ResolveImports() noexcept;
    
    // Обработка экспортов
    bool ParseExportDirectory() const noexcept;
    ExportInfo CreateExportInfo(UInt32 ordinal, UInt32 rva, UInt16 ord_base, 
                              UInt32 va, const std::string& name, FARPROC address) const noexcept;
};

// Утилиты для работы с PE
namespace PEUtils {
    bool IsValidDOSHeader(const IMAGE_DOS_HEADER* header) noexcept;
    bool IsValidNTHeaders(const IMAGE_NT_HEADERS* headers) noexcept;
    bool IsSupportedMachine(WORD machine) noexcept;
    const IMAGE_NT_HEADERS* GetNTHeaders(const void* base) noexcept;
    const IMAGE_SECTION_HEADER* GetFirstSection(const IMAGE_NT_HEADERS* headers) noexcept;
    const IMAGE_SECTION_HEADER* GetSection(const IMAGE_NT_HEADERS* headers, UInt32 index) noexcept;
}

// Глобальные утилиты
namespace Utils {
    std::string FormatAddress(void* address) noexcept;
    std::string FormatOrdinal(UInt16 ordinal) noexcept;
    void PrintExportTable(const std::vector<ExportInfo>& exports) noexcept;
    void PrintModuleInfo(const MemoryModule& module) noexcept;
}

} // namespace MemoryModule

// C-интерфейс для совместимости
extern "C" {
    // Основные функции
    MemoryModule::MemoryModule* memory_module_create() noexcept;
    void memory_module_destroy(MemoryModule::MemoryModule* module) noexcept;
    bool memory_module_load(MemoryModule::MemoryModule* module, const void* data, size_t size) noexcept;
    FARPROC memory_module_get_proc_address(MemoryModule::MemoryModule* module, const char* name) noexcept;
    bool memory_module_unload(MemoryModule::MemoryModule* module) noexcept;
    bool memory_module_is_64bit(MemoryModule::MemoryModule* module) noexcept;
    
    // Функции для работы с экспортами
    size_t memory_module_get_export_count(MemoryModule::MemoryModule* module) noexcept;
    void memory_module_get_export_list(MemoryModule::MemoryModule* module, 
                                      MemoryModule::ExportInfo* exports, 
                                      size_t* count) noexcept;
    FARPROC memory_module_get_proc_address_by_ordinal(MemoryModule::MemoryModule* module, 
                                                     MemoryModule::UInt16 ordinal) noexcept;
    const char* memory_module_get_function_name(MemoryModule::MemoryModule* module, 
                                               MemoryModule::UInt16 ordinal) noexcept;
    MemoryModule::UInt16 memory_module_get_function_ordinal(MemoryModule::MemoryModule* module, 
                                             const char* name) noexcept;
}

// Restore warnings for MSVC
#ifdef XMEMMOD_MSVC
    #pragma warning(pop)
#endif