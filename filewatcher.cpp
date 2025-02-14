#include <dnf5/iplugin.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <cstring>
#include <libdnf5/conf/config_parser.hpp>
#include <libdnf5/transaction/transaction.hpp>

namespace fs = std::filesystem;
using namespace dnf5;

namespace {

// Константы плагина
constexpr const char * PLUGIN_NAME{"filewatcher"};
constexpr PluginVersion PLUGIN_VERSION{ .major = 1, .minor = 0, .micro = 0 };
constexpr PluginAPIVersion REQUIRED_PLUGIN_API_VERSION{ .major = 2, .minor = 0 };

class FileWatcherPlugin : public IPlugin {
private:
    std::string tracked_package;
    std::vector<std::pair<std::string, std::string>> files_to_copy;
    std::vector<bool> overwrite_flags;
    Context * context_ptr{ nullptr };

    // Если debug_mode_ == true, выводятся все сообщения
    bool debug_mode_{ false };

    // Флаг, чтобы finish() выполнялся только один раз
    bool finished_{ false };

    // Вспомогательная функция для вывода информационных сообщений
    void log_debug(const std::string & msg) const {
        if (debug_mode_)
            std::cout << "[filewatcher] " << msg << std::endl;
    }

    // Вспомогательная функция для вывода сообщений об ошибках
    void log_error(const std::string & msg) const {
        if (debug_mode_)
            std::cerr << "[filewatcher] " << msg << std::endl;
    }

    // Загружает конфигурацию из файла /etc/dnf/dnf5-plugins/filewatcher.conf
    void load_config() {
        const std::string config_path = "/etc/dnf/dnf5-plugins/filewatcher.conf";

        if (!fs::exists(config_path)) {
            throw std::runtime_error("Конфигурационный файл отсутствует");
        }

        libdnf5::ConfigParser parser;
        try {
            parser.read(config_path);
        } catch (const std::exception & e) {
            throw std::runtime_error("Ошибка загрузки конфигурации: " + std::string(e.what()));
        }

        // Определяем режим отладки, если в [main] указан debug (любое значение)
        if (parser.has_option("main", "debug")) {
            std::string debug_val = parser.get_value("main", "debug");
            if (!debug_val.empty()) {
                debug_mode_ = true;
            }
        }

        if (parser.has_option("main", "package"))
            tracked_package = parser.get_value("main", "package");
        else
            throw std::runtime_error("Отсутствует параметр package");

        // Секция [files]: ожидается, что записи имеют вид:
        // files[0].source, files[0].destination, files[0].overwrite, и т.д.
        size_t index = 0;
        while (true) {
            std::string src_key = "files[" + std::to_string(index) + "].source";
            std::string dest_key = "files[" + std::to_string(index) + "].destination";
            std::string overwrite_key = "files[" + std::to_string(index) + "].overwrite";

            if (!parser.has_option("files", src_key) || !parser.has_option("files", dest_key))
                break;

            std::string src = parser.get_value("files", src_key);
            std::string dest = parser.get_value("files", dest_key);
            std::string overwrite_str = parser.get_value("files", overwrite_key);
            if (overwrite_str.empty())
                overwrite_str = "false";

            files_to_copy.emplace_back(src, dest);
            overwrite_flags.push_back(overwrite_str == "true");
            index++;
        }

        if (files_to_copy.empty())
            throw std::runtime_error("Отсутствуют записи о файлах для копирования");
    }

    // Функция копирования файлов согласно конфигурации
    void copy_files() {
        for (size_t i = 0; i < files_to_copy.size(); ++i) {
            const auto & pair = files_to_copy[i];
            try {
                if (!fs::exists(pair.first)) {
                    log_error("Файл не найден: " + pair.first);
                    continue;
                }
                if (fs::exists(pair.second) && !overwrite_flags[i]) {
                    log_debug("Файл уже существует, пропускаем: " + pair.second);
                    continue;
                }
                fs::copy(pair.first, pair.second, fs::copy_options::overwrite_existing);
                log_debug("Скопирован " + pair.first + " -> " + pair.second);
            } catch (const std::exception & e) {
                log_error("Ошибка копирования: " + std::string(e.what()));
            }
        }
    }

public:
    // Конструктор плагина
    FileWatcherPlugin(Context & context) : IPlugin(context) {
        context_ptr = &context;
        load_config();
        if (tracked_package.empty())
            throw std::runtime_error("Отсутствует отслеживаемый пакет");
        log_debug("Плагин загружен, отслеживаем пакет: " + tracked_package);
    }

    void init() override {
        // Дополнительная инициализация, если потребуется
    }

    // Метод finish() вызывается после завершения транзакции.
    // Здесь, независимо от состояния транзакции, выполняется копирование файлов.
    void finish() noexcept override {
        if (finished_)
            return;
        finished_ = true;
        try {
            auto transaction = context_ptr->get_transaction();
            if (!transaction) {
                log_error("Транзакция не найдена.");
                return;
            }
            // Здесь не обращаемся к списку пакетов – просто выполняем копирование файлов
            log_debug("Завершается транзакция. Выполняем копирование файлов.");
            copy_files();
        } catch (const std::exception & e) {
            log_error("Исключение в finish(): " + std::string(e.what()));
        } catch (...) {
            log_error("Неизвестное исключение в finish().");
        }
    }

    // Обязательные методы IPlugin:
    PluginAPIVersion get_api_version() const noexcept override {
        return REQUIRED_PLUGIN_API_VERSION;
    }
    const char * get_name() const noexcept override {
        return PLUGIN_NAME;
    }
    PluginVersion get_version() const noexcept override {
        return PLUGIN_VERSION;
    }
    const char * const * get_attributes() const noexcept override {
        static const char * attrs[] = { "author.name", "author.email", "description", nullptr };
        return attrs;
    }
    const char * get_attribute(const char * attribute) const noexcept override {
        if (std::strcmp(attribute, "author.name") == 0)
            return "Your Name";
        else if (std::strcmp(attribute, "author.email") == 0)
            return "your.email@example.com";
        else if (std::strcmp(attribute, "description") == 0)
            return "FileWatcher Plugin. Копирует файлы при обновлении/даунгрейде указанного пакета.";
        return nullptr;
    }
    std::vector<std::unique_ptr<Command>> create_commands() override {
        return {};
    }
};

} // end anonymous namespace

// Функции с C-связью для загрузки плагина:
extern "C" {

PluginAPIVersion dnf5_plugin_get_api_version(void) {
    return REQUIRED_PLUGIN_API_VERSION;
}

const char * dnf5_plugin_get_name(void) {
    return PLUGIN_NAME;
}

PluginVersion dnf5_plugin_get_version(void) {
    return PLUGIN_VERSION;
}

IPlugin * dnf5_plugin_new_instance([[maybe_unused]] ApplicationVersion application_version, Context & context) try {
    return new FileWatcherPlugin(context);
} catch (const std::exception & e) {
    std::cerr << "[filewatcher] Ошибка при создании плагина: " << e.what() << std::endl;
    return nullptr;
} catch (...) {
    std::cerr << "[filewatcher] Неизвестная ошибка при создании плагина." << std::endl;
    return nullptr;
}

void dnf5_plugin_delete_instance(IPlugin * plugin_object) {
    delete plugin_object;
}

} // extern "C"
