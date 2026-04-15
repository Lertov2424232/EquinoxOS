<div align="center">

# EquinoxOS: Хобби-ОС на базе x86

[![License](https://img.shields.io/github/license/ewasion137/EquinoxOS?style=for-the-badge&color=orange)](LICENSE)
[![Status](https://img.shields.io/badge/Status-Active%20Development-green?style=for-the-badge)]()
[![Arch](https://img.shields.io/badge/Architecture-x86__64%20%2F%20i686-blue?style=for-the-badge)]()
[![Toolchain](https://img.shields.io/badge/Toolchain-GCC%20%2F%20NASM-lightgrey?style=for-the-badge)]()

*EquinoxOS - это любительская операционная система с монолитной архитектурой, ориентированная на x86. Данный репозиторий содержит исходный код ядра, драйверы и базовое пользовательское окружение.*

[English](README.md) • Русский

</div>

## Технические характеристики

### Основная архитектура

  * **Целевая платформа:** x86_64 / i686
  * **Загрузчик:** Limine
  * **Формат исполняемых файлов:** ELF (внешний запуск реализован командой `run`. На данный момент запускает нестабильную версию Змейки.)
  * **Инструментарий:** GCC (MinGW-w64 / x86_64-elf-gcc), NASM

### Реализованные подсистемы

  * **Видео:** Драйвер VESA Framebuffer с поддержкой аппаратного курсора.
  * **Ввод:** Драйвер клавиатуры PS/2 (поддержка ASCII-символов и модификатора Shift).
  * **Окружение:** Базовая командная оболочка (Shell) с поддержкой оконного режима.
  * **Сеть:** Тестирование начального стека протоколов (в процессе разработки).

## Сборка и развертывание

### Окружение для разработки

Для проекта требуется кросс-компилятор и ассемблер NASM. Эмуляция выполняется через QEMU.

### Инструкции по сборке

Очистка, полная пересборка и запуск:

```bash
make cleanrun
```

Стандартная сборка:

```bash
make build
make iso
```

### Запуск

Быстрый запуск в QEMU:

```bash
make run
```

## Отладка и диагностика

Для отладки на уровне ядра используйте следующие диагностические флаги QEMU:

```bash
qemu-system-x86_64.exe -cdrom equos.iso -d int,cpu_reset -no-reboot -no-shutdown 2> qemu_log.txt
```

Для получения символов из указателя инструкций:

```bash
x86_64-elf-addr2line -e kernel.elf <address>
```

## Статус проекта

ОС находится в стадии активной разработки. В процессе написания кода автор использует вспомогательные инструменты на базе LLM.

<img width="1278" height="801" alt="image" src="https://github.com/user-attachments/assets/5c9ab047-cd60-42a9-904e-8b5c63db58eb" />
<img width="464" height="581" alt="image" src="https://github.com/user-attachments/assets/52dd9b4d-a635-423a-b7bc-64052a4fc246" />
<img width="455" height="565" alt="image" src="https://github.com/user-attachments/assets/9e21c077-ce9d-4bbc-8865-e1cd939073c6" />

***

Отдельное огромное спасибо моему другу @oxtiskz