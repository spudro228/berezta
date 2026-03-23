# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Beresta — терминальный текстовый редактор на C++17. Поддерживает UTF-8, мультикурсор, закреплённые выделения, поиск/замену, undo/redo, copy/paste (системный clipboard), JSON-подсветку/форматирование. Работает с кодировками UTF-8, Latin-1, CP1251 (автоопределение). Поддерживает Kitty keyboard protocol для работы горячих клавиш при любой раскладке.

## Build & Test

```bash
# Сборка (из корня проекта)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Запуск редактора
./build/beresta [file]

# Запуск всех тестов
cd build && ctest --output-on-failure

# Запуск конкретного теста по фильтру
cd build && ./beresta_tests --gtest_filter='BufferTest.*'
```

Зависимости (utfcpp, googletest) скачиваются автоматически через CMake FetchContent. Vendored: `src/vendor/wcwidth.c`.

## Architecture

Два слоя с чёткой границей:

- **`src/core/`** — логика редактора без платформенных зависимостей. Полностью тестируема через `tests/`.
  - `Buffer` — хранит текст как `vector<string>` (строки без `\n`), всё внутри UTF-8. Codec используется только при файловом I/O.
  - `Selection` (Range + multi-cursor) — инвариант: ranges отсортированы, не пересекаются, auto-merge.
  - `Document` — связывает Buffer, Selection, Viewport, History. Все операции редактирования проходят через Document.
  - `History` — undo/redo через полные снимки буфера (O(file_size) на операцию, для небольших файлов).
  - `Codec` — абстрактный интерфейс кодировок + фабрика `detect_codec`/`create_codec`.
  - `Command` — enum всех команд редактора + `CommandEvent` с payload.

- **`src/tui/`** — терминальный UI, зависит от core.
  - `App` — главный цикл: ввод → dispatch(CommandEvent) → render. Содержит состояние закреплённых выделений (`PinnedItem`), clipboard, режимы ввода.
  - `Input` — маппинг клавиш в `CommandEvent`. Поддержка Kitty keyboard protocol (CSI u) с fallback на legacy control codes. ЙЦУКЕН→QWERTY маппинг для терминалов без полной поддержки Kitty.
  - `Renderer` — отрисовка буфера, подсветка выделений (синий), закреплённых областей (зелёный), JSON-токенов, search matches. Правая панель закреплённых выделений.
  - `Terminal` — raw mode, ANSI escape sequences, Kitty protocol enable/disable.

Тесты покрывают только core-слой (buffer, codec, selection, document, history, search, json, utf8_util).

## Key subsystems

### Закреплённые выделения (Pinned selections)
- `PinnedItem` (app.h) — хранит Range + текстовый снимок + флаг валидности подсветки.
- При редактировании буфера `adjust_pinned_ranges(edit_from, edit_to, delta)` сдвигает ranges после правки, инвалидирует пересекающиеся. Undo/Redo полностью инвалидируют.
- Новые элементы вставляются в начало вектора (новейшие сверху).

### Clipboard
- Системный: `pbcopy`/`pbpaste` на macOS, `xclip` на Linux (через `popen`).
- Внутренний: `clipboard_` в App как fallback.

### Kitty keyboard protocol
- Включается при старте (`try_enable_kitty_keyboard`), выключается при выходе.
- Флаги: 1 (disambiguate) | 4 (report alternate keys) = 5.
- `parse_csi_u()` в input.cpp обрабатывает CSI u последовательности, использует `base_layout_key` для layout-independent хоткеев.
- Fallback: `cyrillic_to_latin()` маппинг ЙЦУКЕН→QWERTY когда base_layout_key отсутствует.

## Conventions

- Namespace: `beresta`.
- Все позиции в Buffer — байтовые смещения в UTF-8, не codepoints.
- Display-ширина (терминальные колонки) считается через `wcwidth` для double-width символов.
- Тесты: Google Test, один файл `tests/<module>_test.cpp` на каждый модуль core.
- InputMode enum определяет режимы ввода: Normal, Search, Replace, PinnedList, Help.
- Все команды редактора определены в `Command` enum (command.h), dispatch в app.cpp.
