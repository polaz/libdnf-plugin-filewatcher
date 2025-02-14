#!/bin/bash

# Путь к директории с проектом
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Удаляем старую папку build, если она есть
rm -rf "$PROJECT_DIR/build"

# Создаём новую папку build и переходим в неё
mkdir "$PROJECT_DIR/build"
cd "$PROJECT_DIR/build" || exit 1

# Запускаем CMake и Make
cmake ..
make -j$(nproc)

# Проверяем, собрался ли файл
if [ -f "$PROJECT_DIR/build/libfilewatcher.so" ]; then
    # Копируем и переименовываем файл в filewatcher.so
    sudo cp "$PROJECT_DIR/build/libfilewatcher.so" /usr/lib64/dnf5/plugins/filewatcher.so
    echo "[✅] Плагин успешно собран и установлен!"
else
    echo "[❌] Ошибка: файл libfilewatcher.so не был создан!"
    exit 1
fi
