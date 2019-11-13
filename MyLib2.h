//
// Created by tester on 10.11.2019.
//
// Use type error error_t

#ifndef MYLIB2_H
#define MYLIB2_H

#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdlib>
#include <cassert>
#include <sys/sysinfo.h>
#include <cerrno>
#include <cstring>
#include <ctype.h>

#define LOCATION __FILE__, __LINE__, __PRETTY_FUNCTION__
#define PRINT_ERROR(message) printf("%s. ERRNO == %s", message, strerror(errno)); printf("\n%s(%d) ERROR in %s\n", LOCATION);
#define PRINT_WARNING(message) printf("\nwarning %s(%d) IN %s: %s\n", LOCATION, message);


FILE *open_file (const char *name, unsigned long *file_size, bool UNIX = false) {
    // Размер файла выдаётся в байтах
    // --------------------------------------------------------------------
    // В системе UNIX можно сделать проверку на существование и на
    // доступность чтения данного файла перед тем, как его открыть, и при
    // помощи fseek и ftell узнать размер файла (Переменной UNIX передать true).
    // Но для кроссплатформенности нужно использовать fstat (UNIX == false)
    // То есть: !(UNIX) => UNIX == false
    // --------------------------------------------------------------------

    enum CONST_ACCESS {
        EXIST, EXECUTE,
        WRITE, READ,
        READWRITE
    };

    assert(name != nullptr);
    assert(file_size != nullptr);

    FILE *file = nullptr;
    if (UNIX) { // Узнаём размер будучи в ОС на UNIX

        if ((file = fopen (name, "rb")) != nullptr) {
            fseek (file, 0, SEEK_END);
            *file_size = ftell (file);
            fseek (file, 0, SEEK_SET);
        } else {
            PRINT_ERROR ("\n" "ERROR open_file UNIX:")

            if (access (name, READ) && access (name, READWRITE)) {    // Проверим доступность файла
                printf ("\n" "ERROR open_file UNIX: "
                        "the file %s is ", name);
                if (access (name, EXIST))
                    printf ("NOT PRESENT\n");
                else
                    printf ("access is denied\n");
            }
        }

    } else {    // Узнаём размер будучи в ОС на Windows

        if ((file = fopen (name, "rb")) != nullptr) {
            struct stat file_specification = {};
            if (!fstat (fileno (file), &file_specification)) {
                *file_size = file_specification.st_size;
            } else {
                printf ("\n" "ERROR open_file: %s nullptr \n", name);
            }
        } else {
            printf ("\n" "ERROR open_file: "
                    "the file %s is either not present or access is denied\n", name);
            perror ("meow");
            perror ("\n" "ERROR open_file:\n");
        }
    }
    return file;
}

namespace rftb {
    enum Read_File_To_Buffer_CONST_ERRORS {
        ALL_RIGHT,
        EMPTY_FILE,
        DONT_OPEN_FILE,
        READ_ERROR,
        DONT_HAVE_FREE_MEMORY
    };
}

char *Read_File_To_Buffer (const char *name, size_t *size, error_t *state_func, bool TEXT, bool UNIX) {
    // Сам очистит буффер при ошибке
    // В конце ставит \0 если TEXT == true, причём размер увеличивается на байт
    // state_func == 0 ошибки отсутствуют
    // state_func == 1 файл пустой
    // state_func == 2 ошибка чтения или записи в файл
    // state_func == 3 не хватает оперативной памяти для считывания текста

    const unsigned amount_of_free_RAM = 100; //MB

    assert (name != nullptr);
    assert (state_func != nullptr);

    unsigned long file_size = 0;
    bool error_read = false;
    FILE *file = open_file (name, &file_size, UNIX);

    if (file == nullptr) {
        *state_func = rftb::DONT_OPEN_FILE;
        return nullptr;
    } else if (file_size == 0) {
        *state_func = rftb::EMPTY_FILE;
        fclose (file);
        return nullptr;
    }

    // Проверка на наличие RAM для buf
    struct sysinfo info = {}; // The toopenkiy CLion cannot handle it :((
    sysinfo(&info);
    unsigned long asdfa = info.freeram;
    if (info.freeram - file_size < amount_of_free_RAM * (1024 * 1024)) {
        PRINT_ERROR("Read_File_To_Buffer: ERROR Not enough RAM for reading text");
        fclose (file);
        *state_func = rftb::DONT_HAVE_FREE_MEMORY;
        return nullptr;
    }

    // В buf будет храниться весь файл name + знак '\0', если TEXT == true
    char *buf = (char *) calloc (file_size + TEXT, sizeof (char));
    if (fread (buf, sizeof (char), file_size, file) != file_size) {
        if (feof (file)) {
            printf ("Read_File_To_Buffer: Error fread file %s\n"
                    "feof(%s) == 1\n", name, name);
            error_read = true;
        } else if (ferror ((file))) {
            printf ("Read_File_To_Buffer: Error fread file %s\n"
                    "ferror(%s) == 1\n", name, name);
            error_read = true;
        }
    }

    fclose (file);

    if (error_read) {
        *state_func = rftb::READ_ERROR;
        PRINT_ERROR ("fread: READ_ERROR")
        free (buf);
        return nullptr;
    }

    if (TEXT) {
        buf[file_size] = '\0';
        *size = ++file_size;
    } else {
        *size = file_size;
    }
    *state_func = rftb::ALL_RIGHT;
    return buf;
}

void bprintf (__uint8_t c) {
    __uint8_t a[8];
    for (int i = 0; i < 8; i++, c /= 2)
        a[i] = c % 2;
    for (int i = 7; i >= 0; i--)
        printf ("%d", a[i]);
}

struct word_t {
    char *word;
    __uint16_t len;
};

namespace gtwds {
    enum returns {
        ALL_RIGHT,
        ERR_CRT_NEW_BUF,
        ERR_COPY_BUF,
        ERR_CRT_WORDS_ARR,

    };
}

error_t get_words (char *buf, size_t size, word_t **words, char **new_buf, __uint32_t *quantity_words) {
    // Создаёт копию buf, обрабатывает её с последующим уменьшением размеров и оставляет
    // её в памяти. Записывает указатели на слова в buf в words.word
    // В words.len лежит длина слова words.word без учёта '\0'
    // Эта функция просто игнорирует небуквенные символы, а слова записывает через '\0'
    // Возвращается статус ошибки (0, если ошибок нет)

    assert(buf != nullptr);
    assert (size > 0);

    if ((*new_buf = (char *) calloc (size, 1)) == nullptr) {
        PRINT_ERROR ("Failed to create new buffer for copy input buffer")
        return gtwds::ERR_CRT_NEW_BUF;
    }

    if (memcpy (*new_buf, buf, size) == nullptr) {
        PRINT_ERROR ("Failed to copy buf in new_buf. Maybe overflow limit memory")
        free (new_buf);
        return gtwds::ERR_COPY_BUF;
    }

    if ((*words = (word_t *) calloc (size / 2, sizeof (word_t))) == nullptr) {
        PRINT_ERROR ("Failed to create words array")
        free (new_buf);
        return gtwds::ERR_CRT_WORDS_ARR;
    }

    char *old_new_buf = *new_buf;
    __uint32_t new_size = 0, quant_words = 0;
    __uint16_t size_word = 0;
    bool ignore_symb = true;
    (*new_buf)--;
    char *read = *new_buf;
    char *write = *new_buf;
    while (*(++read) != '\0' && --size != 0)
        if (isalpha (*read)) {
            *(++write) = tolower (*read);
            new_size++;
            size_word++;
            if (ignore_symb) {
                (*words)[quant_words++].word = write;
                ignore_symb = false;
            }
        } else if (!ignore_symb) {
            *(++write) = '\0';            // Разделитель слов
            (*words)[quant_words - 1].len = size_word;
            size_word = 0;
            new_size++;
            ignore_symb = true;
        }

    if (!ignore_symb) {
        *(++write) = '\0';
        new_size++;
    }

    (*words)[quant_words - 1].len = strlen ((*words)[quant_words - 1].word);

    *quantity_words = quant_words;
    *new_buf = old_new_buf;

    return gtwds::ALL_RIGHT;
}

#endif //MYLIB2_H
