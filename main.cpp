#include "MyLib2.h"
#include <clocale>
#include <climits>
#include <cmath>
#include <ctime>
#include <cstring>
#include <cstdarg>
#include <string>
#include <iostream>
#include <sys/ioctl.h>

#include "bmp.h"

#define FRAME_RATE_PER_SECOND 5
#define NUMBER_TALK_PHOTO 2

namespace bigarr {
    // База данных блочной формы (полностью динамическая)
    // При добавлении строки не забудьте прибавить к длине 1, чтобы учёлся символ '\0'
    typedef char block_arr_t;

    typedef struct rtr_t {      // Тип элемента массива указателей на большие ячейки памяти
        size_t free_mem;        // Количество свободных байт в ячейке
        block_arr_t *begin;     // Указатель на начало ячейки
        block_arr_t *block;     // Указатель на начало свободного места в ячейке
    } router_t;

    typedef struct BlockArray {
    public:
        unsigned quant_rtr;
        size_t size_block;
        router_t *rtr;

        // Устанавливает размер больших ячеек памяти и
        // начальный размер массива указателей на эти массивы
        BlockArray (size_t size_block, unsigned quant_rtr);

        block_arr_t *add (block_arr_t *data, size_t size);

        void dump (FILE *file);

        ~BlockArray ();

    private:
        error_t add_block (unsigned num_block);


    } blkarr_t;

    BlockArray::BlockArray (size_t size_block, unsigned quant_rtr) {
        // При ошибке errno  != 0
        this->quant_rtr = quant_rtr;
        this->size_block = size_block;

        if ((this->rtr = (router_t *) calloc (quant_rtr, sizeof (router_t))) == nullptr) {
            PRINT_ERROR ("Constructor error: failed to create router array")
            return;
        }

        if ((this->rtr->block = (block_arr_t *) calloc (size_block, sizeof (block_arr_t))) == nullptr) {
            PRINT_ERROR ("Constructor error: failed to first block create")
            free (this->rtr);
            return;
        }

        this->rtr->begin = this->rtr->block;
        this->rtr->free_mem = size_block;
    }

    BlockArray::~BlockArray () {
        for (size_t i = 0; i < this->quant_rtr && this->rtr[i].begin != nullptr; i++)
            free (this->rtr[i].begin);
        free (this->rtr);
    }

    block_arr_t *BlockArray::add (block_arr_t *data, size_t size) { // Возращает указатель на выделенную память
        if (size > this->size_block) {
            PRINT_ERROR ("Memory request too large")
            return nullptr;
        }

        unsigned i = 0;
        for (i = 0; i <= quant_rtr; i++) {
            if (this->rtr[i].block == nullptr || i == quant_rtr)
                if (add_block (i))
                    return nullptr;

            if (this->rtr[i].free_mem >= size) {
                if (memcpy (this->rtr[i].block, data, size) == nullptr) {
                    PRINT_ERROR ("Error to copy data")
                    return nullptr;
                }

                // TODO не получается (char *) this->rtr[i].block = (char *) this->rtr[i].block + size;
                block_arr_t *temp_pt = this->rtr[i].block;
                this->rtr[i].block += size / sizeof (block_arr_t);
                this->rtr[i].free_mem -= size;
                return temp_pt;
            }
        }

        PRINT_ERROR ("Overflow Array (overflow memory)")
        return nullptr;
    }

// Добавляет block и при необходимости ещё и расширяет router
    error_t BlockArray::add_block (unsigned num_block) {
        // Если закончился массив из указателей на блоки (переполнился router)
        if (num_block >= this->quant_rtr) {
            router_t *temp = nullptr;
            if ((temp = (router_t *) realloc (this->rtr, 2 * this->quant_rtr * sizeof (router_t))) == nullptr) {
                PRINT_ERROR ("Error realloc router")
                return -1;
            }
            this->quant_rtr *= 2;
            this->rtr = temp;
        }

        if ((this->rtr[num_block].begin = (block_arr_t *) calloc (size_block, sizeof (block_arr_t))) == nullptr) {
            PRINT_ERROR ("Failed to create block array")
            return -1;
        }

        this->rtr[num_block].block = this->rtr[num_block].begin;
        this->rtr[num_block].free_mem = size_block;

        return 0;
    }

    void BlockArray::dump (FILE *file) {
        fprintf (file, R"(///////////////DUMP\\\\\\\\\\\\\\\)" "\n");
        for (unsigned i = 0; i < this->quant_rtr && this->rtr[i].begin != nullptr; i++) {
            fprintf (file, "\n%d\n", i);
            if (fwrite (this->rtr[i].begin, 1, this->size_block, file) != this->size_block) {
                PRINT_ERROR ("Error Dump")
                return;
            }
        }
        fprintf (file, "\n\n");
    }
}

namespace tree {
    typedef char *data_t;

    enum consts {
        RIGHT,
        LEFT,
    };

    enum errors {
        ALL_RIGHT,
        EFILLROOT,
        EFILLNODE,
        EREAD,
        EADDRIGHT,
        EADDLEFT,

    };

    typedef struct nodeTree {
    public:
        data_t data;
        nodeTree *prev;
        nodeTree *right;
        nodeTree *left;

        // Аргумент aux_arg может исопльзоваться для передачи, например указателя на дерево этого узла
        int visitor_pref (int (*func) (nodeTree *, void *), void *aux_arg = nullptr);
    } node_t;

    // Визитор для строк, так как делает проверку на nullptr в data
    // Справа налево
    int nodeTree::visitor_pref (int (*func) (nodeTree *, void *), void *aux_arg) {
        if (data != nullptr) {
            int state = 0;

            if ((state = func (this, aux_arg)) != 0)
                return state;

            if (right != nullptr && (state = right->visitor_pref (func, aux_arg)) != 0)
                return state;

            if (left != nullptr && (state = left->visitor_pref (func, aux_arg)) != 0)
                return state;

        } else {
            PRINT_ERROR ("Empty data")
            return -1;
        }

        return 0;
    }

    // Дерево саморасширяемо, но при этом не уменьшает свои размеры обратно
    typedef struct Tree {
        node_t *node;       // Указатель на корень
        size_t count;       // Количесво узлов

        double exp = 1.5;   // При переполнении дерева, массив увеличится во столько раз
        unsigned step = 0;  // Если после конструктора step > 0, то вместо exp используется шаг step

        void *base_data = nullptr;

        Tree (size_t count, unsigned step);

        int verificator ();

        inline node_t *add_right (node_t *in_node, data_t value);

        inline node_t *add_left (node_t *in_node, data_t value);

        int read_tree_from_file (char *source, bigarr::BlockArray *array);

        int save_to_file (char *name_file);

    private:
        node_t *add (int right_or_left, node_t *curr_node, data_t value);

        node_t *empty_node; // Указатель на свободный элемент узла
        node_t *get_new_node ();

        size_t min_count ();
    } tree_t;

/*  TODO Другая версия конструктора (кажется хуже, чем действующая)
    Tree::Tree (size_t _count, unsigned _step = 0) : count (_count), step (_step) {
        if (count < min_count () && step <= 0) count = min_count ();

        if ((node = (node_t *) calloc (count, sizeof (node_t))) == nullptr) {
            PRINT_ERROR ("Constructor error: failed to create node array")
            return;
        }
        empty_node = node + 1;
        node->left = node->right = node->prev = nullptr;
        assert (verificator ());
    }
*/
    Tree::Tree (size_t _count, unsigned _step = 0) {
        if (_count < min_count () && _step <= 0) _count = min_count ();

        if ((node = (node_t *) calloc (_count, sizeof (node_t))) == nullptr) {
            PRINT_ERROR ("Constructor error: failed to create node array")
            return;
        }
        count = _count;
        empty_node = node + 1;
        node->left = node->right = node->prev = nullptr;
        step = _step;
        assert (verificator ());
    }

    int Tree::verificator () {
        assert (exp >= 0);
        assert (exp || step);
        assert (exp || step > 0);
        assert (count > 0);
        assert (node != nullptr);
        assert (node->prev == nullptr);   // Предком корня может быть только nullptr

        return 1;
    }

    node_t *Tree::add_right (node_t *in_node, data_t value) {
        return add (RIGHT, in_node, value);
    }

    node_t *Tree::add_left (node_t *in_node, data_t value) {
        return add (LEFT, in_node, value);
    }

    node_t *Tree::add (int right_or_left, node_t *curr_node, data_t value) {
        // Не используется удобное добавление непосредственно и использованием БД
        assert (verificator ());
        node_t *new_node = nullptr;                         // Указатель новый узел
        node_t **temp_ptr = nullptr;                        // Указатель для определения выбора режима вставки

        if (right_or_left == RIGHT)
            temp_ptr = &(curr_node->right);
        else
            temp_ptr = &(curr_node->left);

        if ((new_node = get_new_node ()) == nullptr)
            return nullptr;                                 // Критическая ошибка

        new_node->data = value;
        new_node->prev = curr_node;

        // Вставить между узлами новый узел
        // А его left и right указывают на следующий элемент (right == left)
        // сделать проверку на этот случай
        if ((new_node->left = new_node->right = *temp_ptr) != nullptr) {

            if (curr_node->left == curr_node->right)            // Для случая, когда в исходном
                new_node->left = new_node->right = nullptr;     // дереве указатели на left и right были равны
            else
                (*temp_ptr)->prev = new_node;
        }

        *temp_ptr = new_node;
        return new_node;
    }

    node_t *Tree::get_new_node () {
        assert (verificator ());
        if (empty_node == node + count) {   // Если дерево переполнилось, то расширяем его
            typeof (count) new_size = count;
            node_t *new_tree_root = nullptr;

            if (step > 0) {                 // При задании нового размера, проверяется корректность
                new_size += step;           // критически важных параметров дерева
            } else if (exp > 1) {
                new_size = (size_t) ((double) new_size * exp);
            } else {
                errno = EINVAL;
                PRINT_ERROR ("Tree: CRITICAL ERROR: exp < 1")
                return nullptr;
            }

            if ((new_tree_root = (node_t *) realloc (node, new_size * sizeof (node_t))) == nullptr) {
                PRINT_ERROR ("Tree: CRITICAL ERROR: failed to realloc tree")
                return nullptr;
            }

            node = new_tree_root;
            empty_node = new_tree_root + count;
            count = new_size;
        }

        return empty_node++;
    }

    inline size_t Tree::min_count () {
        return floor (1 / (exp - 1) + 1);
    }

    int Tree::read_tree_from_file (char *source, bigarr::BlockArray *array) {
        /* Использую bigarr
         * Уже использую %n)))
         * Не использую %n в sscanf (), потому что размер русского символа два байта, а не один в англ. кодировке.
         * Проблема в том, что внутри строки могут быть числа, которые придётся выделять и т.п.
         * Лучше просто использовать strlen и радоваться жизни
        */
        // right -> yes (first {})
        char str[512] = "";
        bool error = false;
        int state = 0, brk_count = 0;   // Счётчик фигурных скобок (исп. для окончания цикла и поиска синт. ошибок)

        int n = 0;                      // Получает номер окончания считывания, чтобы сдвигать source

        // Заполняем корень
        if ((state = sscanf (source, R"({ "%[^"]" %n)", str, &n)) > 0) {
            source += n;
            brk_count++;

            if ((node->data = array->add (str, strlen (str) + 1)) == nullptr)
                return tree::EFILLROOT;

        } else {
            PRINT_ERROR ("Error read from source")
            return tree::EREAD;
        }

        tree::node_t *curr_node = node;
        bigarr::block_arr_t *tmp = nullptr;     // Используется для проверки успешности выделения памяти
        bool right = true;                      // true: записывается в right, иначе в left (бин. дерево)
        // Заполняем все узлы
        do {
            if (sscanf (source, R"({ "%[^"]" %n)", str, &n) > 0) {
                source += n;
                brk_count++;

                if ((tmp = array->add (str, strlen (str) + 1)) == nullptr)                  // Добавляем строку в БД
                    return tree::EFILLNODE;

                if (right) {
                    if ((curr_node = add_right (curr_node, tmp)) == nullptr)      // Добавляем строку в правую ветвь
                        return tree::EADDRIGHT;

                } else {
                    if ((curr_node = add_left (curr_node, tmp)) == nullptr) {     // Добавляем строку в левую ветвь
                        right = true;
                        return tree::EADDLEFT;
                    }
                    right = true;
                }

            } else if (sscanf (source, "} %n", &n) == 0 && n > 0) {
                source += n;
                brk_count--;
                right = false;

                curr_node = curr_node->prev;
            } else {
                error = true;
            }

        } while (!error && brk_count != 0);

        if (error) {
            PRINT_ERROR ("Error read")
            return tree::EREAD;
        }

        this->base_data = array;
        return 0;
    }

    int save_tree_to_file_vis (tree::node_t *curr_node, void *file) {
        fprintf ((FILE *) file, "{ \"%s\" ", curr_node->data);
        fflush ((FILE *) file);
        if (curr_node->right != nullptr) {
            if (curr_node->right->visitor_pref (save_tree_to_file_vis, file) < 0)
                return -1;
        }
        if (curr_node->left != nullptr) {
            if (curr_node->left->visitor_pref (save_tree_to_file_vis, file) < 0)
                return -1;
        }

        fprintf ((FILE *) file, "} ");
        return 1;
    }

    int Tree::save_to_file (char *name_file) {
        FILE *file = nullptr;
        if ((file = fopen (name_file, "w")) == nullptr) {
            PRINT_ERROR ("Error open file")
            return 0;
        }
        if (node->visitor_pref (save_tree_to_file_vis, stdout) < 0) {
            PRINT_ERROR ("Error save to file")
            return 0;
        }
        return 0;
    }
}

void Dump_Tree (FILE *file_out, tree::node_t *head);

void Dump_Tree_img (tree::node_t *head);

int dialog (tree::tree_t *tree);

inline int spec_printf (tree::node_t *node, void *);

int main () {
    setlocale (LC_ALL, "ru_RU.utf8");
    bigarr::BlockArray array (8192, 16);
    tree::Tree tree (500);

    char str_tree[] = R"({ "Любишь есть горячую еду?" { "А там она обычно полезная?" { "В общаге ФАКИ?" { "Тройка" } { "Там есть чизбургеры?" { "Макдоналдс" } { "КСП" } } } { "Можно отравиться?" { "Шестёрка" } { "Шаурма" } } } { "Супермаркет?" { "Маленький?" { "Пятёрочка" } { "Лента" } } { "Время есть" } } })";

    char tree_save[] = "save_tree.txt";

    if (tree.read_tree_from_file (str_tree, &array)) {
        PRINT_ERROR ("Failed to read tree from file")
        return 0;
    }
    if (dialog (&tree)) {
        PRINT_ERROR ("Error in dialog")
        return 0;
    }

    Dump_Tree_img (tree.node);

    if (tree.save_to_file (tree_save)) {
        PRINT_ERROR ("Error save to file")
        return 0;
    }
    return 0;
}

int multi_strcmp (char *src, char *first, ...) {
    va_list args;
    va_start (args, first);

    if (strcmp (src, first) == 0)
        return 0;

    char *arg = nullptr;
    while ((arg = va_arg (args, char *)) != nullptr)
        if (strcmp (src, arg) == 0)
            return 0;

    return -1;
}

#define YES "Да", "да", "Ага", "ага", "Угу", "угу", "Агась", "агась", nullptr       // Только однословные
#define NO "Нет", "нет", "не", "Неа", "неа", "Нее", "нее", nullptr                  // Только однословные
#define VIEW_AI true
#define SPEAK false

enum tell_type {
    HELLO,
    TALK
};

void _draw (char *cmd, unsigned short wight_cmd) {
    char temp[wight_cmd + 1] = "";
    BitMap example_bmp (cmd);
    int height = example_bmp.height (), width = example_bmp.width ();

    char frame[height * width * 2 + 1 + height] = "";

    int num_pix = 0;
    for (int y = 1; y < height; y++) {
        for (int x = 1; x < width; x++) {
            if ((example_bmp.getPixel (x, y))[0]) {
                frame[num_pix++] = ' ';
                frame[num_pix++] = ' ';
            } else {
                frame[num_pix++] = '#';
                frame[num_pix++] = '#';
            }
        }
        frame[num_pix++] = '\n';
    }

    for (unsigned short i = 0; i < wight_cmd; i++)
        temp[i] = '\n';
    temp[wight_cmd] = '\0';

    if (fwrite (temp, 1, wight_cmd + 1, stdout) != wight_cmd + 1) {
        PRINT_ERROR ("Error send the frame to stdout 1")
        return;
    }

    if (fwrite (frame, 1, num_pix, stdout) != num_pix) {
        PRINT_ERROR ("Error send the frame to stdout 0")
        return;
    }
}

int dialog_TALK (double time) {                     // В секундах
    static bool first_start = false;
    static unsigned short length_cmd = 0;           // Столбцы
    static unsigned short wight_cmd = 0;            // Строки

    char name_folder[] = "TALK";
    char cmd[512] = "";
    if (!first_start) {
        // Получаю размер консоли
        struct winsize size = {0};
        ioctl (STDOUT_FILENO, TIOCGWINSZ, &size);

        length_cmd = size.ws_col / 2;               // Столбцы
        wight_cmd = size.ws_row;                    // Строки

        //unsigned short length_cmd = 130;          // Столбцы
        //unsigned short wight_cmd = 50;            // Строки

        for (int i = 0; i < NUMBER_TALK_PHOTO; i++) {
            sprintf (cmd, "convert %s/%d.png -threshold 45000 -scale %hdx%hd -depth 1 %s/_%d.bmp;",
                     name_folder, i, length_cmd, wight_cmd, name_folder, i);
            printf ("%s\n", cmd);
            system (cmd);
        }
        first_start = true;
    }

    int total_frame = (int) (ceil (time * FRAME_RATE_PER_SECOND));
    for (int frame = 0; frame < total_frame - 1; frame++) {
        sprintf (cmd, "%s/_%d.bmp", name_folder, frame % NUMBER_TALK_PHOTO);
        _draw (cmd, wight_cmd);
        usleep (1000000 / FRAME_RATE_PER_SECOND);
    }
    sprintf (cmd, "%s/_%d.bmp", name_folder, 0);
    _draw (cmd, wight_cmd);
    return 0;
}

void tell (char *str, int tell_type = 0) {
    if (VIEW_AI) {
        static bool first_start = false;
        static FILE *file_tmp = nullptr;

        char name_sound[] = "temp_tell_sound.wav";
        char name_time[] = "temp_tell_time.txt";

        char cmd[512] = "";

        if (!first_start) {
            // Открываем файл, чтобы перехватывать вывод system ()
            if ((file_tmp = fopen (name_time, "w")) == nullptr) {
                PRINT_ERROR ("Error open tmp file")
                return;
            }
            fclose (file_tmp);
            if ((file_tmp = fopen (name_time, "r")) == nullptr) {
                PRINT_ERROR ("Error open tmp file")
                return;
            }


            first_start = true;
        }

        // Записываем речь в файл
        sprintf (cmd, "echo \"%s\" | text2wave -o %s; ", str, name_sound);

        // Получаем время его воспроизведения
        sprintf (cmd, "%s soxi -D %s", cmd, name_sound);

        fflush (nullptr);
        if ((file_tmp = popen (cmd, "r")) == nullptr) {
            PRINT_ERROR ("Cannot execute command")
            return;
        }

        size_t len = 0;
        char *result = nullptr;
        if (getline (&result, &len, file_tmp) == -1) {
            PRINT_ERROR ("Error read temp time file")
            return;
        }

        *(strchr (result, '.')) = ',';   // Из-за русской локали
        double time = atof (result);
        printf ("!!!!!!!!!!\n");
        sprintf (cmd, "aplay %s 2> /dev/null &", name_sound);
        printf ("&&&&&&&&&&\n");
        system (cmd);

        dialog_TALK (time);

    } else if (SPEAK) {
        char cmd[512] = "";
        sprintf (cmd, "echo \"%s\" | festival --tts", str);
        system (cmd);
    } else {
        printf ("%s", str);
        fflush (stdout);
    }
};

int dialog_vis (tree::node_t *node, void *aux_arg) {
    char answer[256] = "";
    char answer_out[256] = "";

    if (node->left == nullptr && node->right == nullptr) {
        sprintf (answer_out, "Это %s, я правильно понимаю?\n", node->data);
        tell (answer_out);

        do {
            if (scanf ("%s", answer) > 0) {
                if (multi_strcmp (answer, YES) == 0) {
                    sprintf (answer_out, "Я надеюсь, ты не сомневался в моих силах...");
                    tell (answer_out);
                    return 1;
                } else if (multi_strcmp (answer, NO) == 0) {
                    // Добавляем новый вопрос
                    sprintf (answer_out, "У всех бывают взлёты и падения, просто сегодня не мой день\n");
                    sprintf (answer_out, "%sВ таком случае напиши свой ответ, который ты хотел услышать\n",
                             answer_out);
                    tell (answer_out);

                    if (std::cin.getline (answer, 256) && std::cin.getline (answer, 256)) {

                        auto *tree = (tree::tree_t *) aux_arg;  // Подключаемся к исходному дереву
                        // Подключаемся к его БД и добавляем новое предложение
                        tree::data_t new_leaf = nullptr;
                        if ((new_leaf = ((bigarr::blkarr_t *) tree->base_data)->add (answer,
                                                                                     strlen (answer) + 1)) ==
                            nullptr) {
                            PRINT_ERROR ("Failed to get memory for new node")
                            return -1;
                        }

                        if (tree->add_right (node, new_leaf) == nullptr ||
                            tree->add_left (node, node->data) == nullptr) {
                            PRINT_ERROR ("Failed to add new node in to tree")
                            return -1;
                        }

                        sprintf (answer_out, "А чем же отличается %s от %s?\n", answer, node->data);
                        tell (answer_out);

                        if (std::cin.getline (answer, 255)) {
                            sprintf (answer, "%s?", answer);
                            if ((node->data = ((bigarr::blkarr_t *) tree->base_data)->
                                    add (answer, strlen (answer) + 1)) == nullptr) {
                                PRINT_ERROR ("Failed to get memory for new node")
                                return -1;
                            }
                        } else {
                            PRINT_ERROR ("Error input")
                            return -1;
                        }

                        sprintf (answer_out,
                                 "Ого, интересно!\nНу что же, теперь я знаю немного больше. Спасибо.\n");
                        tell (answer_out);
                        return 1;
                    } else {
                        PRINT_ERROR ("Error input")
                        return -1;
                    }

                } else {
                    sprintf (answer_out, "Так, я тебя не понял, давай ещё раз. Да или нет\n");
                    tell (answer_out);
                }
            } else {
                PRINT_ERROR ("Error input")
                return -1;
            }
        } while (true);
    }

    sprintf (answer_out, "%s\n", node->data);
    tell (answer_out);

    do {
        if (scanf ("%s", answer) > 0) {
            if (multi_strcmp (answer, YES) == 0) {
                return node->right->visitor_pref (dialog_vis, aux_arg) + 1;
            } else if (multi_strcmp (answer, NO) == 0) {
                return node->left->visitor_pref (dialog_vis, aux_arg) + 1;
            } else {
                sprintf (answer_out, "Так, я тебя не понял, давай ещё раз. Да или нет\n");
                tell (answer_out);
            }
        } else {
            PRINT_ERROR ("Error input")
            return -1;
        }
    } while (true);
}

#undef YES
#undef NO
#undef SPEAK
#undef VIEW_AI

int dialog (tree::tree_t *tree) {
    char name_AI[] = "Ден Рен";
    double boot_time = 0.2;             // Seconds


    printf ("AI started");
    fflush (stdout);

    for (int i = 0; i < 3; i++) {
        usleep (boot_time * 1000000 / 3);
        printf (".");
        fflush (stdout);
    }
    printf ("\n\n");

    // Начало разговора
    tell ("Здравствуй! Спорим, я смогу вычислить, где ты кушаешь? \n"
          "Давай начинать...\n\n");

    if (tree->node->visitor_pref (dialog_vis, tree) < 0) {
        PRINT_ERROR ("Error dialog")
    }

    return 0;
}

namespace Dtdot {

    int ident_dot (tree::node_t *node, void *) {
        if (printf ("\t%zu [label = \"%s\"];\n", (size_t) node, node->data) > 0)
            return 0;
        return -1;
    }

    int build_tree_dot (tree::node_t *node, void *) {
        if (node->right != nullptr)
            printf ("\t%zu -> %zu [label = \"да\"]\n", (size_t) node, (size_t) node->right);

        if (node->left != nullptr)
            printf ("\t%zu -> %zu [label = \"нет\"]\n", (size_t) node, (size_t) node->left);
        return 0;
    }

    inline void recreate_dot_dir () {
        system ("rm -rf TreeSnapshot/ 2> /dev/null");
        system ("mkdir TreeSnapshot");
        system ("touch TreeSnapshot/README.txt; echo This folder constatly deleted! > TreeSnapshot/README.txt");
    }

    bool fill_dot_file (tree::node_t *head) {
        FILE *file_dot = fopen ("temp_file.dot", "w");
        if (file_dot == nullptr) {
            PRINT_ERROR ("DONT CREATE FILE_DOT")
            return false;
        }

        fprintf (file_dot, "digraph G {\n"
                           "    rankdir = TR;\n"
                           "    node[shape=ellipse, fontsize=50, color = red];\n"
                           "    edge[fontsize=50, color = blue, fillcolor = blue];\n");

        FILE *save_stdout = stdout;
        stdout = file_dot;                      // Крайне удобный приём для перенаправления потока

        head->visitor_pref (ident_dot);         // Идентифицирую каждый узел и связываю его с его содержимым

        fprintf (file_dot, "\n");

        head->visitor_pref (build_tree_dot);    // Строю дерево

        fprintf (file_dot, "}");

        fclose (file_dot);
        stdout = save_stdout;
        return true;
    }
}

void Dump_Tree (FILE *file_out, tree::node_t *head) {
    FILE *temp_stdout = stdout;
    stdout = file_out;

    if (head->visitor_pref (spec_printf) < 0) {
        PRINT_ERROR ("Failed to visitor")
    }
    stdout = temp_stdout;
}

void Dump_Tree_img (tree::node_t *head) {
    static size_t number_calls = 0;
    static bool first = false;

    // Удаляю папку со старыми данными и создаю пустую новую для хранения результата (фото, видео, GIF)
    if (!first) {
        Dtdot::recreate_dot_dir ();
        first = true;
    }

    if (!Dtdot::fill_dot_file (head))
        return;

    char comand[256] = "";
    sprintf (comand, "dot -Tpng -Gsize=10,16\\! -Gdpi=150 temp_file.dot -o TreeSnapshot/%zu.png", number_calls);
    system (comand);

    number_calls++;
}

inline int spec_printf (tree::node_t *node, void *) {
    printf ("%s %p\n", node->data, node->data);

    if (node->right != nullptr)
        printf ("\tright: %s\n", node->right->data);

    if (node->left != nullptr)
        printf ("\tleft: %s\n", node->left->data);

    return 0;
}






















