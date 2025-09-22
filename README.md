# xMemMod - Memory Module Library

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-Windows-blue.svg)](https://www.microsoft.com/windows)
[![Architecture](https://img.shields.io/badge/Architecture-x86%20%7C%20x64-green.svg)](https://en.wikipedia.org/wiki/X86-64)

**xMemMod** - это мощная C++ библиотека для загрузки DLL (Dynamic Link Libraries) непосредственно из памяти без записи на диск. Библиотека поддерживает как x86, так и x64 архитектуры и предоставляет полный доступ к экспортируемым функциям с готовыми указателями.

## ✨ Основные возможности

- 🚀 **Загрузка DLL из памяти** - без записи на диск
- 🎯 **Готовые указатели на функции** - немедленное использование
- 🔍 **Полный парсинг экспортов** - автоматическое обнаружение всех функций
- 🏗️ **Поддержка архитектур** - x86 и x64
- 🔧 **C-интерфейс** - совместимость с другими языками
- 🛡️ **Безопасность** - корректная обработка релокаций и импортов
- ⚡ **Производительность** - оптимизированная загрузка и парсинг

## 🚀 Быстрый старт

### Подключение библиотеки

```cpp
#include "xMemMod.h"

// Создание экземпляра
MemoryModule::MemoryModule module;

// Загрузка DLL из памяти
if (module.LoadFromMemory(dll_data, dll_size)) {
    // DLL успешно загружена!
}
```

### Получение списка всех экспортов

```cpp
// Получение всех экспортируемых функций
auto exports = module.GetExportList();

for (const auto& exp : exports) {
    std::cout << "Function: " << exp.name 
              << " (Ordinal: " << exp.ordinal 
              << ", Address: 0x" << std::hex << exp.address << ")" << std::endl;
}
```

### Поиск конкретной функции

```cpp
// Поиск по имени
FARPROC func = module.GetProcAddress("MyFunction");

// Поиск по ординалу
FARPROC func = module.GetProcAddressByOrdinal(1);

// Получение имени функции по ординалу
std::string name = module.GetFunctionName(1);

// Получение ординала по имени
uint16_t ordinal = module.GetFunctionOrdinal("MyFunction");
```

## 📋 API Reference

### MemoryModule Class

#### Основные методы

| Метод | Описание |
|-------|----------|
| `LoadFromMemory(const void* data, size_t size)` | Загружает DLL из байтового массива |
| `GetProcAddress(const char* name)` | Возвращает указатель на функцию по имени |
| `GetExportList()` | Возвращает полный список всех экспортов |
| `Unload()` | Освобождает загруженный модуль |
| `Is64Bit()` | Определяет архитектуру модуля |

#### Дополнительные методы

| Метод | Описание |
|-------|----------|
| `GetProcAddressByOrdinal(uint16_t ordinal)` | Поиск функции по ординалу |
| `GetFunctionName(uint16_t ordinal)` | Получение имени функции по ординалу |
| `GetFunctionOrdinal(const char* name)` | Получение ординала по имени |
| `GetExportCount()` | Количество экспортируемых функций |
| `GetModuleName()` | Имя модуля |
| `GetBaseAddress()` | Базовый адрес загруженного модуля |
| `GetImageSize()` | Размер образа в памяти |

### ExportInfo Structure

```cpp
struct ExportInfo {
    uint32_t ordinal;        // Порядковый номер
    uint32_t rva;           // RVA (Relative Virtual Address)
    uint16_t ordinal_base;  // База ординалов
    uint32_t va;            // Virtual Address
    std::string name;       // Имя функции
    FARPROC address;        // ГОТОВЫЙ указатель на функцию
};
```

## 🔧 C-интерфейс

Для использования в других языках программирования предоставляется C-интерфейс:

```c
// Создание модуля
MemoryModule* module = memory_module_create();

// Загрузка DLL
bool success = memory_module_load_from_memory(module, data, size);

// Получение функции
FARPROC func = memory_module_get_proc_address(module, "MyFunction");

// Получение списка экспортов
ExportInfo* exports;
size_t count;
memory_module_get_export_list(module, &exports, &count);

// Освобождение
memory_module_destroy(module);
```

## 📁 Структура проекта

```
xMemMod/
├── xMemMod.h          # Основной заголовочный файл
├── xMemMod.cpp        # Реализация библиотеки
├── example.cpp        # Демонстрационный пример
├── README.md          # Документация
└── LICENSE            # Лицензия MIT
```

## 🛠️ Требования

- **ОС**: Windows 7/8/10/11
- **Компилятор**: MSVC 2019+, Clang, GCC
- **Архитектура**: x86, x64
- **C++**: C++17 или выше

## 📦 Установка

1. Скопируйте `xMemMod.h` и `xMemMod.cpp` в ваш проект
2. Подключите заголовочный файл: `#include "xMemMod.h"`
3. Скомпилируйте `xMemMod.cpp` вместе с вашим проектом

## 🎯 Примеры использования

### Пример 1: Загрузка WinDivert DLL

```cpp
#include "xMemMod.h"
#include <iostream>

int main() {
    MemoryModule::MemoryModule module;
    
    // Загрузка WinDivert DLL из памяти
    if (module.LoadFromMemory(WinDivertDll, WinDivertDllSize)) {
        std::cout << "DLL загружена успешно!" << std::endl;
        
        // Получение функции WinDivertOpen
        auto WinDivertOpen = module.GetProcAddress("WinDivertOpen");
        if (WinDivertOpen) {
            std::cout << "WinDivertOpen найден: 0x" 
                      << std::hex << WinDivertOpen << std::endl;
        }
        
        // Получение всех экспортов
        auto exports = module.GetExportList();
        std::cout << "Найдено функций: " << exports.size() << std::endl;
    }
    
    return 0;
}
```

### Пример 2: Работа с сетевым драйвером

```cpp
class NetworkDriver {
private:
    MemoryModule::MemoryModule module_;
    
public:
    bool LoadDriver(const void* dll_data, size_t size) {
        return module_.LoadFromMemory(dll_data, size);
    }
    
    bool OpenHandle() {
        auto WinDivertOpen = module_.GetProcAddress("WinDivertOpen");
        if (!WinDivertOpen) return false;
        
        // Использование функции
        // HANDLE handle = WinDivertOpen(filter, layer, priority, flags);
        return true;
    }
};
```

## 🔒 Безопасность

- ✅ Корректная обработка релокаций
- ✅ Валидация PE-структур
- ✅ Безопасное освобождение памяти
- ✅ Проверка архитектуры
- ✅ Обработка исключений

## 📄 Лицензия

Этот проект распространяется под лицензией MIT. См. файл [LICENSE](LICENSE) для подробностей.

## 🤝 Вклад в проект

Мы приветствуем вклад в развитие проекта! Пожалуйста:

1. Форкните репозиторий
2. Создайте ветку для новой функции
3. Внесите изменения
4. Создайте Pull Request

## 📞 Поддержка

Если у вас есть вопросы или проблемы:

- Создайте [Issue](https://github.com/Platon7788/xMemMod/issues)
- Опишите проблему подробно
- Приложите пример кода, если возможно

## 🏆 Особенности

- **Высокая производительность** - оптимизированная загрузка
- **Полная совместимость** - работает с любыми PE-файлами
- **Простота использования** - интуитивный API
- **Надежность** - тщательно протестированная библиотека
- **Документация** - подробные примеры и API reference

---

**xMemMod** - делаем загрузку DLL из памяти простой и безопасной! 🚀
