#include "MyLib2.h"
#include <clocale>
#include <climits>
#include <cmath>

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

        int visitor_pref (int (*func) (nodeTree *));
    } node_t;

    // Визитор для строк, так как делает проверку на nullptr в data
    // Слева направо
    int nodeTree::visitor_pref (int (*func) (node_t *)) {
        if (data != nullptr) {
            if (func (this) < 0)
                return -1;

            if (right != nullptr && right->visitor_pref (func) == -1)
                return -1;

            if (left != nullptr && left->visitor_pref (func) == -1)
                return -1;

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

        Tree (size_t count, unsigned step);

        int verificator ();

        inline node_t *add_right (node_t *in_node, data_t value);

        inline node_t *add_left (node_t *in_node, data_t value);

        int read_tree_from_file (char *source, bigarr::BlockArray *array);
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

        return 0;
    }
}

inline int spec_printf (tree::node_t *node) {
    printf ("%s %p\n", node->data, node->data);
    if (node->right != nullptr)
        printf ("\tright: %s\n", node->right->data);
    if (node->left != nullptr)
        printf ("\tleft: %s\n", node->left->data);
}

int main () {
    /*
    char str[256] = "";
    char str1[512] = "} { \"КСП\" } } }";
    char c = '\0';
    int n = 0;
    printf ("%d\n", sscanf (str1, "}%c%n", &c, &n));
    printf ("%c %d\n%s\n", c, n, str);
    return 0;*/

    setlocale (LC_ALL, "ru_RU.utf8");
    bigarr::BlockArray array (8192, 16);
    tree::Tree tree (500);

    char str_tree[] = R"({ "Любишь погорячее?" { "Полезная?" { "В общаге ФАКИ?" { "Тройка" } { "Там есть чизбургеры?" { "Макдоналдс" } { "КСП" } } } { "Можно отравиться?" { "Шестёрка" } { "Шаурма" } } } { "Супермаркет?" { "Маленький?" { "Пятёрочка" } { "Лента" } } { "Время есть" } } })";
    //printf ("%s\n", str_tree);
    if (tree.read_tree_from_file (str_tree, &array)) {
        PRINT_ERROR ("Failed to read tree from file")
        return 0;
    }

    if (tree.node->visitor_pref (spec_printf) < 0) {
        PRINT_ERROR ("Failed to visitor")
        return 0;
    }

    return 0;
}




























