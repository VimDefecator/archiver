# Архиватор

## Использование

Бэкенд архиватора состоит из файлов "archive.hh", "archive.cc" и "fileutils.hh".

Интерфейс бэкенда состоит из классов **Archive**, **Folder** и **Entry**.

**Archive** - главный класс, который открывает существующий или создает новый файл архива по указанному пути. Он дает доступ к корневой папке архива через метод **getRootFolder** и позволяет сохранить изменения в файл архива через метод **sync**.

Через интерфейс класса **Folder** можно просматривать содержимое архива (**iterChildren**, **iterChildrenUntil**, **getChildFolder** и **getParentFolder**), добавлять файлы и папки (**addFolder**, **addFile**), удалять файлы и папки (**remove**), а также извлекать файлы (**extract**).

Информация о содержимом архива представлена в виде класса **Entry**, интерфейс которого позволяет узнать тип (**getType**, файл или папка), имя (**getName**) и размер (**getSize**, если это файл).

## Формат архива

                         число|содержимое
                          байт|
                       0 =====|============
    размер файла file1 >     6|abcdef       <-- контент файла file1
                       6 -----|------------
    размер файла file2 >    12|hqunvjqedsvx <-- контент файла file2
                      18 -----|------------
    sizeof(uint16_t) -->     2|0            <-- индекс родительской папки для папки 0 (для корневой - та же самая папка)
                      20 -----|------------
    sizeof(uint16_t) -->     2|2            <-- число файлов и папок в папке 0
                      22 -----|------------
    sizeof(uint8_t) --->     1|0            <-- тип (0 - файл)
                      23 -----|------------
    sizeof(uint64_t) -->     8|0            <-- адрес файла file1
                      31 -----|------------
    sizeof(uint64_t) -->     8|6            <-- размер файла file1
                      39 -----|------------
    strlen("file1")+1 ->     6|file1\0      <-- имя файла + нуль-терминатор
                      45 -----|------------
    sizeof(uint8_t) --->     1|1            <-- тип (1 - папка)
                      46 -----|------------
    sizeof(uint16_t) -->     2|1            <-- индекс папки folder
                      48 -----|------------
    strlen("folder")+1 >     7|folder\0     <-- имя папки + нуль-терминатор
                      55 -----|------------
    sizeof(uint16_t) -->     2|0            <-- индекс родительской папки для папки 1
                      57 -----|------------
    sizeof(uint16_t) -->     2|1            <-- число файлов и папок в папке 1
                      59 -----|------------
    sizeof(uint8_t) --->     1|0            <-- тип (0 - файл)
                      60 -----|------------
    sizeof(uint64_t) -->     8|6            <-- адрес файла file2
                      68 -----|------------
    sizeof(uint64_t) -->     8|12           <-- размер файла file2
                      76 -----|------------
    strlen("file2")+1 ->     6|file2\0      <-- имя файла + нуль-терминатор
                      82 -----|------------
    sizeof(int32_t) --->     4|-82          <-- адрес папки 0 относительно конца
                      86 -----|------------
    sizeof(int32_t) --->     4|-45          <-- адрес папки 1 относительно конца
                      90 -----|------------
    sizeof(uint16_t) -->     2|2            <-- количество папок
                      92 -----|------------
    sizeof(uint64_t) -->     8|18           <-- адрес конца сегмента данных
                     100 -----|------------
