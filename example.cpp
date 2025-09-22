/**
 * @file example.cpp
 * @brief Демонстрационный пример использования библиотеки xMemMod
 * @author xMemMod Team
 * @version 1.0
 * 
 * Этот пример показывает основные возможности библиотеки xMemMod:
 * - Загрузка DLL из памяти
 * - Получение списка экспортов
 * - Поиск функций по имени и ординалу
 * - Работа с готовыми указателями на функции
 */

#include "xMemMod.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

// Простая DLL для демонстрации (заглушка)
// В реальном проекте здесь был бы байтовый массив с DLL
extern const unsigned char SampleDll[];
extern const size_t SampleDllSize;

/**
 * @brief Демонстрация основных возможностей xMemMod
 */
void demonstrateBasicUsage() {
    std::cout << "=== Демонстрация основных возможностей xMemMod ===" << std::endl;
    std::cout << std::endl;
    
    // Создание экземпляра MemoryModule
    MemoryModule::MemoryModule module;
    
    std::cout << "1. Создание MemoryModule экземпляра..." << std::endl;
    std::cout << "   ✓ Экземпляр создан успешно" << std::endl;
    std::cout << std::endl;
    
    // В реальном проекте здесь была бы загрузка DLL
    std::cout << "2. Загрузка DLL из памяти..." << std::endl;
    std::cout << "   ⚠️  В этом примере используется заглушка" << std::endl;
    std::cout << "   В реальном проекте: module.LoadFromMemory(dll_data, dll_size)" << std::endl;
    std::cout << std::endl;
}

/**
 * @brief Демонстрация работы с экспортами
 */
void demonstrateExportHandling() {
    std::cout << "=== Работа с экспортами ===" << std::endl;
    std::cout << std::endl;
    
    MemoryModule::MemoryModule module;
    
    // Симуляция загруженной DLL
    std::cout << "1. Получение информации о модуле:" << std::endl;
    std::cout << "   Архитектура: " << (module.Is64Bit() ? "x64" : "x86") << std::endl;
    std::cout << "   Базовый адрес: 0x" << std::hex << module.GetBaseAddress() << std::dec << std::endl;
    std::cout << "   Размер образа: " << module.GetImageSize() << " байт" << std::endl;
    std::cout << std::endl;
    
    // Демонстрация работы с экспортами (симуляция)
    std::cout << "2. Работа с экспортами:" << std::endl;
    std::cout << "   Количество экспортов: " << module.GetExportCount() << std::endl;
    std::cout << "   Имя модуля: " << module.GetModuleName() << std::endl;
    std::cout << std::endl;
}

/**
 * @brief Демонстрация поиска функций
 */
void demonstrateFunctionSearch() {
    std::cout << "=== Поиск функций ===" << std::endl;
    std::cout << std::endl;
    
    MemoryModule::MemoryModule module;
    
    // Список функций для поиска
    std::vector<std::string> functions_to_find = {
        "CreateFile",
        "ReadFile", 
        "WriteFile",
        "CloseHandle",
        "GetLastError"
    };
    
    std::cout << "1. Поиск функций по имени:" << std::endl;
    for (const auto& func_name : functions_to_find) {
        FARPROC address = module.GetProcAddress(func_name.c_str());
        if (address) {
            std::cout << "   ✓ " << func_name << " -> 0x" 
                      << std::hex << reinterpret_cast<void*>(address) << std::dec << std::endl;
        } else {
            std::cout << "   ✗ " << func_name << " -> НЕ НАЙДЕНА" << std::endl;
        }
    }
    std::cout << std::endl;
    
    // Демонстрация поиска по ординалам
    std::cout << "2. Поиск функций по ординалам:" << std::endl;
    for (uint16_t ordinal = 1; ordinal <= 5; ++ordinal) {
        FARPROC address = module.GetProcAddressByOrdinal(ordinal);
        std::string name = module.GetFunctionName(ordinal);
        uint16_t found_ordinal = module.GetFunctionOrdinal(name.c_str());
        
        std::cout << "   Ординал " << ordinal << " -> " << name 
                  << " (0x" << std::hex << reinterpret_cast<void*>(address) << std::dec << ")" << std::endl;
        std::cout << "   Имя \"" << name << "\" -> Ординал " << found_ordinal << std::endl;
    }
    std::cout << std::endl;
}

/**
 * @brief Демонстрация работы с полным списком экспортов
 */
void demonstrateExportList() {
    std::cout << "=== Полный список экспортов ===" << std::endl;
    std::cout << std::endl;
    
    MemoryModule::MemoryModule module;
    
    std::cout << "1. Получение полного списка экспортов:" << std::endl;
    auto exports = module.GetExportList();
    
    std::cout << "   Найдено экспортов: " << exports.size() << std::endl;
    std::cout << std::endl;
    
    if (!exports.empty()) {
        std::cout << "2. Первые 5 экспортов:" << std::endl;
        std::cout << "   №\tОрдинал\tRVA\t\tИмя\t\t\tАдрес" << std::endl;
        std::cout << "   " << std::string(60, '-') << std::endl;
        
        size_t count = (exports.size() < 5) ? exports.size() : 5;
        for (size_t i = 0; i < count; ++i) {
            const auto& exp = exports[i];
            std::cout << "   " << (i + 1) << "\t"
                      << "0x" << std::hex << exp.ordinal << "\t"
                      << "0x" << std::hex << exp.rva << "\t"
                      << exp.name << "\t\t"
                      << "0x" << std::hex << reinterpret_cast<void*>(exp.address) << std::dec << std::endl;
        }
    }
    std::cout << std::endl;
}

/**
 * @brief Демонстрация C-интерфейса
 */
void demonstrateCInterface() {
    std::cout << "=== C-интерфейс ===" << std::endl;
    std::cout << std::endl;
    
    std::cout << "1. Использование C-интерфейса:" << std::endl;
    std::cout << "   // Создание модуля" << std::endl;
    std::cout << "   MemoryModule* module = memory_module_create();" << std::endl;
    std::cout << std::endl;
    
    std::cout << "   // Загрузка DLL" << std::endl;
    std::cout << "   bool success = memory_module_load_from_memory(module, data, size);" << std::endl;
    std::cout << std::endl;
    
    std::cout << "   // Получение функции" << std::endl;
    std::cout << "   FARPROC func = memory_module_get_proc_address(module, \"MyFunction\");" << std::endl;
    std::cout << std::endl;
    
    std::cout << "   // Освобождение" << std::endl;
    std::cout << "   memory_module_destroy(module);" << std::endl;
    std::cout << std::endl;
}

/**
 * @brief Демонстрация практического использования
 */
void demonstratePracticalUsage() {
    std::cout << "=== Практическое использование ===" << std::endl;
    std::cout << std::endl;
    
    std::cout << "1. Пример класса для работы с DLL:" << std::endl;
    std::cout << "   class MyDllWrapper {" << std::endl;
    std::cout << "   private:" << std::endl;
    std::cout << "       MemoryModule::MemoryModule module_;" << std::endl;
    std::cout << "   public:" << std::endl;
    std::cout << "       bool LoadDll(const void* data, size_t size) {" << std::endl;
    std::cout << "           return module_.LoadFromMemory(data, size);" << std::endl;
    std::cout << "       }" << std::endl;
    std::cout << "       " << std::endl;
    std::cout << "       FARPROC GetFunction(const char* name) {" << std::endl;
    std::cout << "           return module_.GetProcAddress(name);" << std::endl;
    std::cout << "       }" << std::endl;
    std::cout << "   };" << std::endl;
    std::cout << std::endl;
    
    std::cout << "2. Преимущества xMemMod:" << std::endl;
    std::cout << "   ✓ Загрузка DLL без записи на диск" << std::endl;
    std::cout << "   ✓ Готовые указатели на функции" << std::endl;
    std::cout << "   ✓ Автоматический парсинг экспортов" << std::endl;
    std::cout << "   ✓ Поддержка x86 и x64" << std::endl;
    std::cout << "   ✓ C-интерфейс для других языков" << std::endl;
    std::cout << "   ✓ Безопасная обработка памяти" << std::endl;
    std::cout << std::endl;
}

/**
 * @brief Главная функция демонстрации
 */
int main() {
    std::cout << "🚀 xMemMod - Демонстрационный пример" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << std::endl;
    
    try {
        // Демонстрация основных возможностей
        demonstrateBasicUsage();
        
        // Демонстрация работы с экспортами
        demonstrateExportHandling();
        
        // Демонстрация поиска функций
        demonstrateFunctionSearch();
        
        // Демонстрация работы с полным списком экспортов
        demonstrateExportList();
        
        // Демонстрация C-интерфейса
        demonstrateCInterface();
        
        // Демонстрация практического использования
        demonstratePracticalUsage();
        
        std::cout << "✅ Все демонстрации завершены успешно!" << std::endl;
        std::cout << std::endl;
        std::cout << "📚 Для получения дополнительной информации:" << std::endl;
        std::cout << "   - Изучите README.md" << std::endl;
        std::cout << "   - Посмотрите примеры в документации" << std::endl;
        std::cout << "   - Создайте Issue на GitHub для вопросов" << std::endl;
        std::cout << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Ошибка: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Неизвестная ошибка!" << std::endl;
        return 1;
    }
    
    std::cout << "Нажмите Enter для выхода...";
    std::cin.get();
    
    return 0;
}
