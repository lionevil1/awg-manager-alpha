# awg-manager-alpha — Context Guide

## Project Overview

**awg-manager-alpha** — минималистичный C-веб-сервер для роутеров Keenetic (Entware, ARM64/aarch64). Предоставляет:

- Страницу входа с аутентификацией через Keenetic RCI API (challenge-response),
- Локальные сессионные cookie,
- Веб-интерфейс для управления AmneziaWG,
- Интеграцию с ядром (загрузка/выгрузка модуля `amneziawg.ko`),
- Автообновление из GitHub-репозитория.

**Архитектура:**

- Чистый C11, без внешних зависимостей (кроме стандартной библиотеки),
- Встроенный HTTP-сервер ( POSIX-сокеты),
- In-memory хранилище сессий,
- Модульная структура: `server`, `session`, `config`, `hash`, `router_auth`.

## Directory Structure

```
awg-manager-alpha/
├── src/                    # Исходный код C
│   ├── main.c              # Точка входа
│   ├── server.c/h          # HTTP-сервер и роутинг
│   ├── session.c/h         # Управление сессиями
│   ├── config.c/h          # Конфигурация (env-переменные)
│   ├── hash.c/h            # MD5/SHA-256 хеширование
│   └── router_auth.c/h     # RCI аутентификация Keenetic
├── web/                    # Веб-ресурсы (UI)
│   ├── login.html          # Страница входа
│   ├── app.html            # Dashboard
│   └── assets/             # CSS/JS файлы
├── init.d/
│   └── S99awg-manager-alpha  # Init-скрипт Entware
├── scripts/
│   ├── build_aarch64_wsl.sh  # Кросс-компиляция + упаковка
│   ├── build_ipk.sh          # Создание .ipk пакета
│   ├── updater.sh            # Скрипт автообновления
│   └── create_backup.sh      # Чекпоинт-бэкапы
├── packaging/ipk/          # Шаблон IPK-пакета
├── tests/
│   └── test_main.c         # Юнит-тесты
├── update/
│   └── latest-aarch64-3.10.txt  # Манифест обновлений
├── backups/                # Архивы чекпоинтов
├── build/                  # Скомпилированные артефакты
├── dist/                   # IPK-пакеты
└── Makefile                # Сборка на хосте
```

## Building and Running

### Host Build (x86_64)

```sh
make              # Сборка бинарника
make test         # Запуск юнит-тестов
make clean        # Очистка
```

Бинарник: `build/awg-manager-alpha`

### Cross-Compile для роутера (WSL)

```sh
# Установка тулчейна
sudo apt install -y build-essential binutils gcc-aarch64-linux-gnu \
  libc6-dev-arm64-cross file upx-ucl

# Опционально: Zig для статической линковки (рекомендуется)
sudo snap install --beta zig --classic

# Сборка + упаковка + тесты
./scripts/build_aarch64_wsl.sh aarch64-3.10 <version>
```

Результат: `dist/awg-manager-alpha_<version>_aarch64-3.10.ipk`

### Запуск

**Локально (разработка):**

```sh
AWG_WEB_ROOT=./web ./build/awg-manager-alpha
```

**На роутере (после установки IPK):**

```sh
/opt/etc/init.d/S99awg-manager-alpha start
```

Лог: `/opt/var/log/awg-manager-alpha.log`

### Environment Variables

| Переменная | Описание | По умолчанию |
|------------|----------|--------------|
| `AWG_LISTEN_ADDR` | Адрес слушателя | `0.0.0.0` |
| `AWG_LISTEN_PORT` | Порт слушателя | `8088` |
| `AWG_ROUTER_ADDR` | IP роутера | авто-детект |
| `AWG_ROUTER_PORT` | Порт роутера | `80` |
| `AWG_WEB_ROOT` | Путь к веб-ресурсам | `/opt/share/awg-manager-alpha/www` |
| `AWG_MODULE_PATH` | Путь к модулю ядра | `/opt/lib/modules/amneziawg.ko` |
| `AWG_UPDATE_MANIFEST_URL` | URL манифеста обновлений | GitHub raw URL |
| `AWG_SESSION_TTL` | TTL сессии (сек) | `1800` |

**Файл окружения (опционально):** `/opt/etc/awg-manager-alpha.env`

## API Endpoints

### Public

| Метод | Путь | Описание |
|-------|------|----------|
| `GET` | `/` | Редирект на `/login` |
| `GET` | `/login` | Страница входа |
| `POST` | `/api/login` | Аутентификация (Keenetic RCI) |
| `GET` | `/assets/*` | Статические ресурсы |

### Session-Protected

| Метод | Путь | Описание |
|-------|------|----------|
| `GET` | `/app` | Dashboard |
| `GET` | `/api/kernel-status` | Статус модуля ядра |
| `GET` | `/api/update/status` | Статус обновлений |
| `POST` | `/api/update/check` | Проверка обновлений |
| `POST` | `/api/update/apply` | Применение обновления |
| `POST` | `/api/logout` | Завершение сессии |

## Kernel Module Lifecycle

Модуль `amneziawg.ko` управляется через init-скрипт:

- **start**: `insmod`, если не загружен,
- **stop**: `rmmod`,
- **status**: проверка состояния модуля + процесса.

Dashboard отображает индикатор `AmneziaWG Kernel` (зелёный/красный).

## Auto-Update

**Манифест:** `update/latest-aarch64-3.10.txt`

```txt
version=<new-version>
ipk_url=<https-url-to-ipk>
sha256=<optional-sha256>
```

**Скрипт обновления:** `/opt/libexec/awg-manager-alpha/updater.sh`

**Лог обновлений:** `/opt/var/log/awg-manager-alpha-update.log`

## IPK Package Structure

```
/opt/
├── bin/awg-manager-alpha           # Бинарник
├── etc/init.d/S99awg-manager-alpha # Init-скрипт
├── share/awg-manager-alpha/www/    # Веб-ресурсы
├── lib/modules/amneziawg.ko        # Модуль ядра
└── libexec/awg-manager-alpha/
    └── updater.sh                  # Скрипт обновлений
```

## Development Conventions

### Code Style

- **Стандарт:** C11 (`-std=c11 -D_GNU_SOURCE`),
- **Префиксы:** `awg_` для всех публичных функций/типов,
- **Именование:** `snake_case` для функций/переменных, `UPPER_CASE` для макросов,
- **Заголовки:** include guards `AWG_<NAME>_H`,
- **Обработка ошибок:** возврат `-1` при ошибке, `0` при успехе.

### Testing

- Юнит-тесты: `tests/test_main.c`,
- Запуск: `make test`,
- Тесты покрывают: хеши (MD5/SHA-256), сессии, конфигурацию.

### Security

- Пароли не сохраняются в файлах,
- Сессионные токены из `/dev/urandom`,
- Cookie: `HttpOnly; SameSite=Strict; Path=/`,
- Ограничения на размер запроса/тела.

### Versioning

- Формат: `0.1.0-N` (alpha),
- Changelog: `CHANGELOG.md` (Keep a Changelog format),
- Манифест обновлений: `update/latest-aarch64-3.10.txt`.

## Key Files Reference

| Файл | Описание |
|------|----------|
| `src/server.c` | HTTP-сервер, парсинг запросов, роутинг |
| `src/session.c` | In-memory хранилище сессий (128 слотов) |
| `src/router_auth.c` | Keenetic RCI challenge-response аутентификация |
| `src/hash.c` | MD5/SHA-256 реализация (встроенная) |
| `init.d/S99awg-manager-alpha` | Entware init-скрипт с управлением модулем |
| `scripts/build_ipk.sh` | Создание tar-gzip IPK пакета |
| `web/assets/auth.js` | Логика страницы входа |
| `web/assets/app.js` | Логика dashboard |

## Troubleshooting

| Проблема | Решение |
|----------|---------|
| `Not downgrading package` | Проверить имя пакета (`awg-manager-alpha`) |
| `Malformed package file` | Пересобрать IPK через `build_ipk.sh` |
| Login stuck на "Checking..." | Проверить `AWG_ROUTER_ADDR`/`PORT`, сеть |
| `insmod`/`rmmod` ошибки | Проверить путь модуля, права, совместимость ядра |
| Update button disabled | Проверить манифест, лог обновлений |

## Public Repository

- **GitHub:** `https://github.com/lionevil1/awg-manager-alpha`
- **Releases:** `https://github.com/lionevil1/awg-manager-alpha/releases`
