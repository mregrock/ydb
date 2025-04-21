# admin cluster config replace

С помощью команды `admin cluster config replace` вы можете загрузить [динамическую конфигурацию](../../../../../maintenance/manual/dynamic-config.md) на кластер {{ ydb-short-name }}.

{% include [danger-warning](../_includes/danger-warning.md) %}

Общий вид команды:

```bash
ydb [global options...] admin cluster config replace [options...]
```

* `global options` — глобальные параметры.
* `options` — [параметры подкоманды](#options).

Посмотрите описание команды замены динамической конфигурации:

```bash
ydb admin cluster config replace --help
```

## Параметры подкоманды {#options}

#|
|| Имя | Описание ||
|| `-f`, `--filename` | Путь к файлу, содержащему конфигурацию. ||
|| `--allow-unknown-fields`
| Разрешить наличие неизвестных полей в конфигурации.

Если флаг не указан, наличие неизвестных полей в конфигурации приводит к ошибке.
    ||
|| `--ignore-local-validation`
| Игнорировать базовую валидацию конфигурации на стороне клиента.

Если флаг не указан, YDB CLI проводит базовую валидацию конфигурации.
    ||
|#

## Примеры {#examples}

Загрузите файл динамической конфигурации на кластер:

```bash
ydb admin cluster config replace --filename сonfig.yaml
```

Загрузите файл динамической конфигурации на кластер, игнорируя локальные проверки применимости:

```bash
ydb admin cluster config replace -f config.yaml --ignore-local-validation
```

Загрузите файл динамиической конфигурации на кластер, игнорируя проверку конфигурации на неизвестные поля:

```bash
ydb admin cluster config replace -f config.yaml --allow-unknown-fields
```
